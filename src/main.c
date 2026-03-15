/*
 * SCS3304 — One-on-One Chat Application
 * Standalone (Non Client-Server) Version
 *
 * Entry point and main menu logic.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common/user_manager.h"
#include "common/message_handler.h"
#include "common/auth.h"

/* ── currently logged-in username ( when its empty then it means that no user is logged in)*/
static char current_user[MAX_NAME_LEN + 1] = "";

/* 
 * : clear_screen
 *  : Wipe the terminal between menu states
  */
static void clear_screen(void) {
    printf("\033[2J\033[H");
}

// consistent barner at the top of every screen
static void print_header(const char *title) {
    printf("\n  ╔══════════════════════════════════════════╗\n");
    printf("  ║  %-40s║\n", title);
    printf("  ╚══════════════════════════════════════════╝\n\n");
}

static void print_error(const char *msg)   { printf("  [!] %s\n", msg); }
static void print_success(const char *msg) { printf("  [+] %s\n", msg); }
static void print_info(const char *msg)    { printf("  [*] %s\n", msg); }


static void read_line(char *buf, int size) {
    if (fgets(buf, size, stdin) != NULL)
        buf[strcspn(buf, "\n")] = 0;
    else
        buf[0] = 0;
}

/* 
 * SCREEN : do_register
 * PURPOSE : Collect username + password and register a new account
 * */
static void do_register(void) {
    char username[MAX_NAME_LEN + 1];
    char password[64];
    char confirm[64];

    clear_screen();
    print_header("register new account");

    printf("  username (no spaces)  : ");
    read_line(username, sizeof(username));

    printf("  password (min 4 chars): ");
    read_line(password, sizeof(password));

    printf("  confirm password      : ");
    read_line(confirm, sizeof(confirm));

    if (strcmp(password, confirm) != 0) {
        print_error("passwords do not match.");
        return;
    }

    int result = register_user(username, password);

    switch (result) {
        case SUCCESS:
            print_success("account created. you can now log in.");
            break;
        case ERR_DUPLICATE:
            print_error("that username is already taken.");
            break;
        case ERR_INVALID:
            print_error("username cannot be empty and password must be at least 4 characters.");
            break;
        case ERR_FULL:
            print_error("user limit reached. contact the administrator.");
            break;
        default:
            print_error("registration failed. check that data/users.txt is accessible.");
    }
}


static int do_login(void) {
    char username[MAX_NAME_LEN + 1];
    char password[64];

    clear_screen();
    print_header("login");

    printf("  username : ");
    read_line(username, sizeof(username));

    printf("  password : ");
    read_line(password, sizeof(password));

    int result = login_user(username, password);

    switch (result) {
        case SUCCESS:
            strncpy(current_user, username, MAX_NAME_LEN);
            printf("\n  welcome back, %s!\n", current_user);
            return 1;
        case ERR_NOT_FOUND:
            print_error("username not found.");
            return 0;
        case ERR_WRONG_PASS:
            print_error("incorrect password.");
            return 0;
        case ERR_ALREADY_ON:
            print_error("this account is already logged in.");
            return 0;
        default:
            print_error("login failed.");
            return 0;
    }
}

/* 
 *  : do_send_message
 *  : Let the logged-in user pick a recipient and type a message
 *    : 1. List users so sender can see who is available
 *           2. Enter recipient username
 *           3. Enter message body
 *           4. Call send_message()
 * */
static void do_send_message(void) {
    char to[MAX_NAME_LEN + 1];
    char body[MAX_BODY_LEN];

    clear_screen();
    print_header("send message");

    /* show user list so sender can see who to send to */
    list_users();

    printf("\n  send to (username): ");
    read_line(to, sizeof(to));

    if (strcasecmp(to, current_user) == 0) {
        print_error("you cannot send a message to yourself.");
        return;
    }

    printf("  message: ");
    read_line(body, sizeof(body));

    if (strlen(body) == 0) {
        print_error("message cannot be empty.");
        return;
    }

    int result = send_message(current_user, to, body);

    if (result == SUCCESS)
        print_success("message sent.");
    else
        print_error("recipient not found. check the username and try again.");
}

/* 
 * SCREEN : do_inbox
 * PURPOSE : Show all messages received by the current user
 */
static void do_inbox(void) {
    clear_screen();
    print_header("inbox");
    show_inbox(current_user);
}

/* 
 * SCREEN : do_conversation
 * PURPOSE : Show the full message thread with another user
 */
static void do_conversation(void) {
    char other[MAX_NAME_LEN + 1];

    clear_screen();
    print_header("view conversation");

    list_users();

    printf("\n  view conversation with: ");
    read_line(other, sizeof(other));

    if (!user_exists(other)) {
        print_error("user not found.");
        return;
    }

    show_conversation(current_user, other);
}

/* 
 * SCREEN : do_search
 * PURPOSE : Look up a single user and display their status
 */
static void do_search(void) {
    char target[MAX_NAME_LEN + 1];

    clear_screen();
    print_header("search user");

    printf("  enter username to search: ");
    read_line(target, sizeof(target));

    if (!search_user(target))
        print_error("user not found.");
}

/* 
 * SCREEN : do_logout
 * PURPOSE : Mark current user OFFLINE and clear the session
 */
static void do_logout(void) {
    logout_user(current_user);
    printf("\n  [+] %s has been logged out.\n", current_user);
    current_user[0] = '\0';
}

/* 
 * SCREEN : do_deregister
 * PURPOSE : Permanently delete the current user's account
 */
static void do_deregister(void) {
    char confirm[8];

    clear_screen();
    print_header("delete account");

    printf("  this will permanently delete your account.\n");
    printf("  type YES to confirm: ");
    read_line(confirm, sizeof(confirm));

    if (strcmp(confirm, "YES") != 0) {
        print_info("cancelled.");
        return;
    }

    logout_user(current_user);
    deregister_user(current_user);
    print_success("account deleted.");
    current_user[0] = '\0';
}

/* 
 * MENU : logged_in_menu
 * PURPOSE : Main menu displayed after a successful login
  */
static void logged_in_menu(void) {
    int choice;

    while (1) {
        clear_screen();
        printf("\n  ╔══════════════════════════════════════════╗\n");
        printf("  ║  logged in as: %-26s║\n", current_user);
        printf("  ╠══════════════════════════════════════════╣\n");
        printf("  ║  1.  send a message                      ║\n");
        printf("  ║  2.  view inbox                          ║\n");
        printf("  ║  3.  view conversation history           ║\n");
        printf("  ║  4.  search user                         ║\n");
        printf("  ║  5.  list all users                      ║\n");
        printf("  ║  6.  logout                              ║\n");
        printf("  ║  7.  delete my account                   ║\n");
        printf("  ╚══════════════════════════════════════════╝\n");
        printf("  choice: ");

        if (scanf("%d", &choice) != 1) { while (getchar() != '\n'); continue; }
        while (getchar() != '\n');

        switch (choice) {
            case 1: do_send_message();   break;
            case 2: do_inbox();          break;
            case 3: do_conversation();   break;
            case 4: do_search();         break;
            case 5:
                clear_screen();
                print_header("all users");
                list_users();
                break;
            case 6:
                do_logout();
                return;   /* go back to the welcome menu */
            case 7:
                do_deregister();
                if (current_user[0] == '\0') return;
                break;
            default:
                print_error("invalid choice — enter a number from the menu.");
        }

        /* pause so the user can read the output before the screen clears */
        printf("\n  press Enter to continue...");
        getchar();
    }
}

/* 
 * MENU : welcome_menu
 * PURPOSE : First screen shown on launch — login or register
 */
static void welcome_menu(void) {
    int choice;

    // this is the meny it will keep on displaying untill the user choses exit as an option
    while (1) {
        clear_screen();
        printf("\n  ╔══════════════════════════════════════════╗\n");
        printf("  ║      one-on-one chat  —  SCS3304         ║\n");
        printf("  ╠══════════════════════════════════════════╣\n");
        printf("  ║  1.  login                               ║\n");
        printf("  ║  2.  register new account                ║\n");
        printf("  ║  3.  list all users                      ║\n");
        printf("  ║  4.  exit                                ║\n");
        printf("  ╚══════════════════════════════════════════╝\n");
        printf("  choice: ");

        if (scanf("%d", &choice) != 1) { while (getchar() != '\n'); continue; }
        while (getchar() != '\n');

        switch (choice) {
            case 1:
                if (do_login())
                    logged_in_menu();
                else {
                    printf("\n  press Enter to try again...");
                    getchar();
                }
                break;
            case 2:
                do_register();
                printf("\n  press Enter to continue...");
                getchar();
                break;
            case 3:
                clear_screen();
                print_header("all users");
                list_users();
                printf("\n  press Enter to continue...");
                getchar();
                break;
            case 4:
                clear_screen();
                printf("  goodbye.\n\n");
                exit(0);
            default:
                print_error("invalid choice.");
                printf("\n  press Enter to continue...");
                getchar();
        }
    }
}


int main(void) {
    clear_screen();
    printf(" one-on-one chat  —  SCS3304 \n");
    printf(" standalone (non client-server) \n");

  

    welcome_menu();
    return 0;
}