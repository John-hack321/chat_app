#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "server.h"
#include "../common/protocol.h"
#include "../common/utils.h"
#include "../common/user_manager.h"
#include "../common/message_handler.h"

// one slot per connected client 
typedef struct {
    int  fd;                         // socket fd — -1 means empty slot 
    char username[MAX_NAME_LEN + 1]; // empty string means not logged in 
} Client;

static Client clients[MAX_USERS];
static int    client_count = 0;

static Client *find_client(const char *username) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].fd > 0 &&
            strcasecmp(clients[i].username, username) == 0)
            return &clients[i];
    }
    return NULL;
}

// cleans up a disconnected client
// it logs them out and clossed their fd and removes it from the array
static void remove_client(int fd) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].fd != fd) continue;

        if (clients[i].username[0] != '\0') {
            logout_user(clients[i].username);
            printf("  [*] %s disconnected\n", clients[i].username);
        }

        /* shift remaining slots down to fill the gap */
        for (int j = i; j < client_count - 1; j++)
            clients[j] = clients[j + 1];

        client_count--;
        close(fd);
        return;
    }
}


// parses and executes the different types of commands received from a client
static void handle_command(Client *c, char *cmd) {
    char response[BUFFER_SIZE];
    char command[16]        = {0};
    char a1[50]             = {0};
    char a2[50]             = {0};
    char body[MAX_BODY_LEN] = {0};

    // extract command word — everything before the first colon 
    sscanf(cmd, "%15[^:]", command);

    // REGISTER -> REG:username:password 
    if (strcmp(command, CMD_REGISTER) == 0) {
        sscanf(cmd, "%*[^:]:%49[^:]:%49[^\n]", a1, a2);
        int r = register_user(a1, a2);
        if      (r == SUCCESS)       snprintf(response, sizeof(response), CMD_ACK_OK);
        else if (r == ERR_DUPLICATE) snprintf(response, sizeof(response), "%s:username already taken", CMD_ACK_ERR);
        else if (r == ERR_INVALID)   snprintf(response, sizeof(response), "%s:password too short (min 4 chars)", CMD_ACK_ERR);
        else                         snprintf(response, sizeof(response), "%s:server error", CMD_ACK_ERR);
        send_msg(c->fd, response);
    }

    // LOGIN -> LOGIN:username:password 
    else if (strcmp(command, CMD_LOGIN) == 0) {
        sscanf(cmd, "%*[^:]:%49[^:]:%49[^\n]", a1, a2);
        int r = login_user(a1, a2);
        if (r == SUCCESS) {
            strncpy(c->username, a1, MAX_NAME_LEN);
            printf("  [+] %s logged in\n", a1);
            send_msg(c->fd, CMD_ACK_OK);
        } else if (r == ERR_NOT_FOUND)  { snprintf(response, sizeof(response), "%s:user not found",    CMD_ACK_ERR); send_msg(c->fd, response); }
        else if   (r == ERR_WRONG_PASS) { snprintf(response, sizeof(response), "%s:wrong password",    CMD_ACK_ERR); send_msg(c->fd, response); }
        else if   (r == ERR_ALREADY_ON) { snprintf(response, sizeof(response), "%s:already logged in", CMD_ACK_ERR); send_msg(c->fd, response); }
        else                            { snprintf(response, sizeof(response), "%s:login failed",       CMD_ACK_ERR); send_msg(c->fd, response); }
    }

    // LOGOUT -> LOGOUT:username 
    else if (strcmp(command, CMD_LOGOUT) == 0) {
        sscanf(cmd, "%*[^:]:%49[^\n]", a1);
        logout_user(a1);
        printf("  [*] %s logged out\n", a1);
        c->username[0] = '\0';
        send_msg(c->fd, CMD_ACK_OK);
    }

    // DEREGISTER -> DEREG:username 
    else if (strcmp(command, CMD_DEREGISTER) == 0) {
        sscanf(cmd, "%*[^:]:%49[^\n]", a1);
        logout_user(a1);
        deregister_user(a1);
        c->username[0] = '\0';
        send_msg(c->fd, CMD_ACK_OK);
    }

    // LIST ->LIST 
    else if (strcmp(command, CMD_LIST) == 0) {
        build_user_list(response, sizeof(response));
        send_msg(c->fd, response);
    }

    // SEARCH -> SEARCH:username 
    else if (strcmp(command, CMD_SEARCH) == 0) {
        sscanf(cmd, "%*[^:]:%49[^\n]", a1);
        if (user_exists(a1))
            snprintf(response, sizeof(response), "SEARCH_RESULT:%s:%s",
                    a1, is_online(a1) ? STATUS_ONLINE : STATUS_OFFLINE);
        else
            snprintf(response, sizeof(response), "SEARCH_RESULT:not_found");
        send_msg(c->fd, response);
    }

    // SEND MESSAGE -> MSG:from:to:body 
    else if (strcmp(command, CMD_MSG) == 0) {
        
        // Body may contain colons so we read everything after
        // the third colon as one field with %1023[^\n].
        
        sscanf(cmd, "%*[^:]:%49[^:]:%49[^:]:%1023[^\n]", a1, a2, body);

        // persist to file first 
        int r = store_message(a1, a2, body);
        if (r != SUCCESS) {
            snprintf(response, sizeof(response), "%s:recipient not found", CMD_ACK_ERR);
            send_msg(c->fd, response);
            return;
        }

        //  find_client() searches clients[] in this same process.
        //  The fd it returns is immediately usable with send_msg().
        //  No cross-process fd sharing needed.
        // 
        //  We only deliver to the RECIPIENT — not back to the sender.
        //  The sender already echoed their own message locally in
        //  the client's chat loop.
        
        Client *recipient = find_client(a2);
        if (recipient != NULL && recipient->fd != c->fd) {
            char deliver[BUFFER_SIZE];
            snprintf(deliver, sizeof(deliver), "%s:%s:%s",
                    CMD_DELIVER, a1, body);
            send_msg(recipient->fd, deliver);
        }

        // ACK to sender — client chat loop discards this silently 
        send_msg(c->fd, CMD_ACK_OK);
    }

    // INBOX -> INBOX:username 
    else if (strcmp(command, CMD_INBOX) == 0) {
        sscanf(cmd, "%*[^:]:%49[^\n]", a1);
        build_inbox_str(a1, response, sizeof(response));
        send_msg(c->fd, response);
    }

    // RECENT -> RECENT:user_a:user_b 
    else if (strcmp(command, CMD_RECENT) == 0) {
        sscanf(cmd, "%*[^:]:%49[^:]:%49[^\n]", a1, a2);
        build_recent_str(a1, a2, 8, response, sizeof(response));
        send_msg(c->fd, response);
    }

    // SENDERS -> SENDERS:username 
    else if (strcmp(command, "SENDERS") == 0) {
        sscanf(cmd, "%*[^:]:%49[^\n]", a1);
        build_inbox_senders(a1, response, sizeof(response));
        send_msg(c->fd, response);
    }

    // for unknown commands
    else {
        snprintf(response, sizeof(response), "%s:unknown command", CMD_ACK_ERR);
        send_msg(c->fd, response);
    }
}

// here we run the select loop forever
void server_run(void) {
    int server_fd;
    struct sockaddr_in addr;
    int opt = 1;

    // initialise all client slots to empty 
    for (int i = 0; i < MAX_USERS; i++) {
        clients[i].fd = -1;
        clients[i].username[0] = '\0';
    }

    // create TCP socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("  [!] socket() failed"); exit(1); }

    // allow immediate port reuse after server restart
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // bind to 127.0.0.1:8080 
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(SERVER_PORT);
    addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("  [!] bind() failed");
        exit(1);
    }

    // we then listen 
    if (listen(server_fd, BACKLOG) < 0) {
        perror("  [!] listen() failed");
        exit(1);
    }

    printf("  [*] server listening on %s:%d\n", SERVER_IP, SERVER_PORT);
    printf("  [*] concurrency: select() I/O multiplexing\n");
    printf("  [*] press Ctrl+C to stop\n\n");

    // we then enter the select loop untin interupted 
    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);

        FD_SET(server_fd, &read_fds);
        int max_fd = server_fd;

        // add every active client fd to the watch set
        for (int i = 0; i < client_count; i++) {
            if (clients[i].fd > 0) {
                FD_SET(clients[i].fd, &read_fds);
                if (clients[i].fd > max_fd)
                    max_fd = clients[i].fd;
            }
        }

        
        // select() blocks until at least one fd is readable.
        // On return, read_fds only has bits set for the fds that
        // actually have data ready — we check each one below.
        
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0)
            continue;

        // new client connecting 
        if (FD_ISSET(server_fd, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int client_fd = accept(server_fd,
                                   (struct sockaddr *)&client_addr,
                                    &addr_len);
            if (client_fd < 0) {
                perror("  [!] accept() failed");
            } else if (client_count >= MAX_USERS) {
                send_msg(client_fd, "ACK:ERR:server full");
                close(client_fd);
            } else {
                clients[client_count].fd = client_fd;
                clients[client_count].username[0] = '\0';
                client_count++;
                printf("  [+] new client (fd=%d) — %d connected\n",
                        client_fd, client_count);
            }
        }

        for (int i = 0; i < client_count; i++) {
            int fd = clients[i].fd;
            if (fd <= 0 || !FD_ISSET(fd, &read_fds)) continue;

            char buf[BUFFER_SIZE];
            if (recv_msg(fd, buf, sizeof(buf)) < 0) {
                remove_client(fd);
                i--;
            } else {
                handle_command(&clients[i], buf);
            }
        }
    }
}