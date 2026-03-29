/*
 * SCS3304 — One-on-One Chat Application
 * Server Module — select() I/O multiplexing model
 *
 * WHY WE SWITCHED FROM fork() TO select():
 *
 *   The previous fork() model created a child process per client.
 *   Each child had its own memory space, so when child A (alice)
 *   tried to deliver a message to child B (bob) by writing to
 *   bob's socket fd — it failed silently. The fd existed in child
 *   B's memory, not child A's. Real-time delivery was broken.
 *
 * HOW select() FIXES THIS:
 *
 *   All clients are handled in ONE process. All socket fds live
 *   in the same memory. When alice sends to bob, the server looks
 *   up bob's fd in clients[] and writes directly — same process,
 *   same memory, it works instantly.
 *
 * CONCURRENCY MODEL — I/O MULTIPLEXING:
 *
 *   select() watches a SET of file descriptors and returns as
 *   soon as ANY one of them has data ready. The server handles
 *   that fd and immediately loops back to watch all of them again.
 *   From each client's perspective, the server is always listening.
 *   This is genuine concurrency — just event-driven rather than
 *   process-based.
 *
 *   This is the same model used by nginx, Redis, and Node.js.
 *
 * FILE LOCKING — flock():
 *
 *   flock() is kept on all file writes even in single-process mode.
 *   It is good practice and ensures correctness if the program is
 *   ever extended to a multi-process model.
 *
 * THE MAIN LOOP:
 *   1. Build fd_set: listening socket + all connected client fds
 *   2. select() — blocks until any fd has data
 *   3. Listening socket ready → new client → accept() → add to clients[]
 *   4. Client socket ready → read command → handle → respond
 *   5. Client disconnected → remove from clients[] → close fd
 *   6. Repeat
 */

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
 
 /* ── one slot per connected client ── */
 typedef struct {
     int  fd;                         /* socket fd — -1 means empty slot */
     char username[MAX_NAME_LEN + 1]; /* empty string means not logged in */
 } Client;
 
 static Client clients[MAX_USERS];
 static int    client_count = 0;
 
 /* ============================================================
  * FUNCTION : find_client
  * PURPOSE  : Look up a connected client by username.
  *            Returns a pointer to their slot, or NULL.
  *
  *   This is what enables real-time delivery — we find the
  *   recipient's fd directly in our own process memory and
  *   write to it immediately. No cross-process issues.
  * ============================================================ */
 static Client *find_client(const char *username) {
     for (int i = 0; i < client_count; i++) {
         if (clients[i].fd > 0 &&
             strcasecmp(clients[i].username, username) == 0)
             return &clients[i];
     }
     return NULL;
 }
 
 /* ============================================================
  * FUNCTION : remove_client
  * PURPOSE  : Clean up a disconnected client.
  *            Logs them out, closes their fd, removes from array.
  * ============================================================ */
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
 
 /* ============================================================
  * FUNCTION : handle_command
  * PURPOSE  : Parse and execute one command received from a client.
  *
  *   Format:  COMMAND:arg1:arg2:...
  *
  *   The command word is extracted first (everything before the
  *   first colon), then arguments are parsed per command type.
  *
  * INPUT : c   — pointer to the sending client's slot
  *         cmd — the full command string received
  * ============================================================ */
 static void handle_command(Client *c, char *cmd) {
     char response[BUFFER_SIZE];
     char command[16]        = {0};
     char a1[50]             = {0};
     char a2[50]             = {0};
     char body[MAX_BODY_LEN] = {0};
 
     /* extract command word — everything before the first colon */
     sscanf(cmd, "%15[^:]", command);
 
     /* ── REGISTER ── REG:username:password ── */
     if (strcmp(command, CMD_REGISTER) == 0) {
         sscanf(cmd, "%*[^:]:%49[^:]:%49[^\n]", a1, a2);
         int r = register_user(a1, a2);
         if      (r == SUCCESS)       snprintf(response, sizeof(response), CMD_ACK_OK);
         else if (r == ERR_DUPLICATE) snprintf(response, sizeof(response), "%s:username already taken", CMD_ACK_ERR);
         else if (r == ERR_INVALID)   snprintf(response, sizeof(response), "%s:password too short (min 4 chars)", CMD_ACK_ERR);
         else                         snprintf(response, sizeof(response), "%s:server error", CMD_ACK_ERR);
         send_msg(c->fd, response);
     }
 
     /* ── LOGIN ── LOGIN:username:password ── */
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
 
     /* ── LOGOUT ── LOGOUT:username ── */
     else if (strcmp(command, CMD_LOGOUT) == 0) {
         sscanf(cmd, "%*[^:]:%49[^\n]", a1);
         logout_user(a1);
         printf("  [*] %s logged out\n", a1);
         c->username[0] = '\0';
         send_msg(c->fd, CMD_ACK_OK);
     }
 
     /* ── DEREGISTER ── DEREG:username ── */
     else if (strcmp(command, CMD_DEREGISTER) == 0) {
         sscanf(cmd, "%*[^:]:%49[^\n]", a1);
         logout_user(a1);
         deregister_user(a1);
         c->username[0] = '\0';
         send_msg(c->fd, CMD_ACK_OK);
     }
 
     /* ── LIST ── LIST ── */
     else if (strcmp(command, CMD_LIST) == 0) {
         build_user_list(response, sizeof(response));
         send_msg(c->fd, response);
     }
 
     /* ── SEARCH ── SEARCH:username ── */
     else if (strcmp(command, CMD_SEARCH) == 0) {
         sscanf(cmd, "%*[^:]:%49[^\n]", a1);
         if (user_exists(a1))
             snprintf(response, sizeof(response), "SEARCH_RESULT:%s:%s",
                      a1, is_online(a1) ? STATUS_ONLINE : STATUS_OFFLINE);
         else
             snprintf(response, sizeof(response), "SEARCH_RESULT:not_found");
         send_msg(c->fd, response);
     }
 
     /* ── SEND MESSAGE ── MSG:from:to:body ── */
     else if (strcmp(command, CMD_MSG) == 0) {
         /*
          * Body may contain colons so we read everything after
          * the third colon as one field with %1023[^\n].
          */
         sscanf(cmd, "%*[^:]:%49[^:]:%49[^:]:%1023[^\n]", a1, a2, body);
 
         /* persist to file first */
         int r = store_message(a1, a2, body);
         if (r != SUCCESS) {
             snprintf(response, sizeof(response), "%s:recipient not found", CMD_ACK_ERR);
             send_msg(c->fd, response);
             return;
         }
 
         /*
          * Real-time delivery — the key advantage of select().
          *
          * find_client() searches clients[] in this same process.
          * The fd it returns is immediately usable with send_msg().
          * No cross-process fd sharing needed.
          *
          * We only deliver to the RECIPIENT — not back to the sender.
          * The sender already echoed their own message locally in
          * the client's chat loop.
          */
         Client *recipient = find_client(a2);
         if (recipient != NULL && recipient->fd != c->fd) {
             char deliver[BUFFER_SIZE];
             snprintf(deliver, sizeof(deliver), "%s:%s:%s",
                      CMD_DELIVER, a1, body);
             send_msg(recipient->fd, deliver);
         }
 
         /* ACK to sender — client chat loop discards this silently */
         send_msg(c->fd, CMD_ACK_OK);
     }
 
     /* ── INBOX ── INBOX:username ── */
     else if (strcmp(command, CMD_INBOX) == 0) {
         sscanf(cmd, "%*[^:]:%49[^\n]", a1);
         build_inbox_str(a1, response, sizeof(response));
         send_msg(c->fd, response);
     }
 
     /* ── RECENT ── RECENT:user_a:user_b ── */
     else if (strcmp(command, CMD_RECENT) == 0) {
         sscanf(cmd, "%*[^:]:%49[^:]:%49[^\n]", a1, a2);
         build_recent_str(a1, a2, 8, response, sizeof(response));
         send_msg(c->fd, response);
     }
 
     /* ── SENDERS ── SENDERS:username ── */
     else if (strcmp(command, "SENDERS") == 0) {
         sscanf(cmd, "%*[^:]:%49[^\n]", a1);
         build_inbox_senders(a1, response, sizeof(response));
         send_msg(c->fd, response);
     }
 
     /* ── unknown command ── */
     else {
         snprintf(response, sizeof(response), "%s:unknown command", CMD_ACK_ERR);
         send_msg(c->fd, response);
     }
 }
 
 /* ============================================================
  * FUNCTION : server_run
  * PURPOSE  : Bind, listen, then run the select() loop forever.
  * ============================================================ */
 void server_run(void) {
     int server_fd;
     struct sockaddr_in addr;
     int opt = 1;
 
     /* initialise all client slots to empty */
     for (int i = 0; i < MAX_USERS; i++) {
         clients[i].fd = -1;
         clients[i].username[0] = '\0';
     }
 
     /* ── step 1: create TCP socket ── */
     server_fd = socket(AF_INET, SOCK_STREAM, 0);
     if (server_fd < 0) { perror("  [!] socket() failed"); exit(1); }
 
     /* allow immediate port reuse after server restart */
     setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
 
     /* ── step 2: bind to 127.0.0.1:8080 ── */
     memset(&addr, 0, sizeof(addr));
     addr.sin_family      = AF_INET;
     addr.sin_port        = htons(SERVER_PORT);
     addr.sin_addr.s_addr = inet_addr(SERVER_IP);
 
     if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
         perror("  [!] bind() failed");
         exit(1);
     }
 
     /* ── step 3: listen ── */
     if (listen(server_fd, BACKLOG) < 0) {
         perror("  [!] listen() failed");
         exit(1);
     }
 
     printf("  [*] server listening on %s:%d\n", SERVER_IP, SERVER_PORT);
     printf("  [*] concurrency: select() I/O multiplexing\n");
     printf("  [*] press Ctrl+C to stop\n\n");
 
     /* ── step 4: select() loop ── */
     while (1) {
         fd_set read_fds;
         FD_ZERO(&read_fds);
 
         /* always watch the listening socket for new connections */
         FD_SET(server_fd, &read_fds);
         int max_fd = server_fd;
 
         /* add every active client fd to the watch set */
         for (int i = 0; i < client_count; i++) {
             if (clients[i].fd > 0) {
                 FD_SET(clients[i].fd, &read_fds);
                 if (clients[i].fd > max_fd)
                     max_fd = clients[i].fd;
             }
         }
 
         /*
          * select() blocks until at least one fd is readable.
          * On return, read_fds only has bits set for the fds that
          * actually have data ready — we check each one below.
          */
         if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0)
             continue;
 
         /* ── new client connecting ── */
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
 
         /*
          * Check each client for incoming data.
          * Index-based loop because remove_client() shifts the array —
          * we decrement i after removal to re-check the same index.
          */
         for (int i = 0; i < client_count; i++) {
             int fd = clients[i].fd;
             if (fd <= 0 || !FD_ISSET(fd, &read_fds)) continue;
 
             char buf[BUFFER_SIZE];
             if (recv_msg(fd, buf, sizeof(buf)) < 0) {
                 remove_client(fd);
                 i--;   /* compensate for array shift after removal */
             } else {
                 handle_command(&clients[i], buf);
             }
         }
     }
 }