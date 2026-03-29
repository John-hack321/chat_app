#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/file.h>   // so as to access flock() 
#include "user_manager.h"
#include "auth.h"
#include "protocol.h"

// shows what time its is now in utc format 
static void now(char *buf, int size) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(buf, size, "%Y-%m-%d %H:%M", tm_info);
}

// internal: parse one line User struct 
static void parse_line(const char *line, User *u) {
    memset(u, 0, sizeof(User));
    sscanf(line, "%49[^:]:%15[^:]:%9[^:]:%19[^\n]",
            u->username, u->password_hash, u->status, u->last_seen);
}

// internal: load all lines from users.txt into array
static int load_users(char lines[MAX_USERS][128]) {
    FILE *fp = fopen(USERS_FILE, "r");
    if (fp == NULL) return 0;

    flock(fileno(fp), LOCK_SH);     // shared read lock 
    int count = 0;
    while (count < MAX_USERS && fgets(lines[count], 128, fp) != NULL)
        count++;
    flock(fileno(fp), LOCK_UN);     // release 

    fclose(fp);
    return count;
}

//internal: write all lines back to users.txt 
static int save_lines(char lines[MAX_USERS][128], int count) {
    FILE *fp = fopen(USERS_FILE, "w");
    if (fp == NULL) return ERR_FILE;

    flock(fileno(fp), LOCK_EX);     /* exclusive write lock */
    for (int i = 0; i < count; i++)
        fputs(lines[i], fp);
    flock(fileno(fp), LOCK_UN);     /* release */

    fclose(fp);
    return SUCCESS;
}

// checks whether a username is in user.txt
int user_exists(const char *username) {
    char lines[MAX_USERS][128];
    int  count = load_users(lines);
    User u;
    for (int i = 0; i < count; i++) {
        parse_line(lines[i], &u);
        if (strcasecmp(u.username, username) == 0) return 1;
    }
    return 0;
}

// checks whether a user's status is online
int is_online(const char *username) {
    char lines[MAX_USERS][128];
    int  count = load_users(lines);
    User u;
    for (int i = 0; i < count; i++) {
        parse_line(lines[i], &u);
        if (strcasecmp(u.username, username) == 0)
            return (strcmp(u.status, STATUS_ONLINE) == 0);
    }
    return 0;
}

// adds a new users with a hashed password
int register_user(const char *username, const char *password) {
    if (strlen(username) == 0 || strlen(password) < MIN_PASSWORD_LEN)
        return ERR_INVALID;

    char lines[MAX_USERS][128];
    int  count = load_users(lines);
    if (count >= MAX_USERS) return ERR_FULL;

    User u;
    for (int i = 0; i < count; i++) {
        parse_line(lines[i], &u);
        if (strcasecmp(u.username, username) == 0) return ERR_DUPLICATE;
    }

    char hash_str[16], timestamp[MAX_TIME_LEN];
    snprintf(hash_str, sizeof(hash_str), "%u", hash_password(password));
    now(timestamp, sizeof(timestamp));

    // append new user — exclusive lock while writing 
    FILE *fp = fopen(USERS_FILE, "a");
    if (fp == NULL) return ERR_FILE;

    flock(fileno(fp), LOCK_EX);
    fprintf(fp, "%s:%s:%s:%s\n", username, hash_str, STATUS_OFFLINE, timestamp);
    flock(fileno(fp), LOCK_UN);

    fclose(fp);
    return SUCCESS;
}

// verify credential and mark user as online
int login_user(const char *username, const char *password) {
    char lines[MAX_USERS][128];
    int  count = load_users(lines);
    int  found = -1;
    User u;

    for (int i = 0; i < count; i++) {
        parse_line(lines[i], &u);
        if (strcasecmp(u.username, username) == 0) { found = i; break; }
    }
    if (found == -1) return ERR_NOT_FOUND;
    if (!verify_password(password, u.password_hash)) return ERR_WRONG_PASS;
    if (strcmp(u.status, STATUS_ONLINE) == 0) return ERR_ALREADY_ON;

    char timestamp[MAX_TIME_LEN];
    now(timestamp, sizeof(timestamp));
    snprintf(lines[found], 128, "%s:%s:%s:%s\n",
            u.username, u.password_hash, STATUS_ONLINE, timestamp);

    return save_lines(lines, count);
}


// marks user as offline and thus logs out the user
int logout_user(const char *username) {
    char lines[MAX_USERS][128];
    int  count = load_users(lines);
    int  found = -1;
    User u;

    for (int i = 0; i < count; i++) {
        parse_line(lines[i], &u);
        if (strcasecmp(u.username, username) == 0) { found = i; break; }
    }
    if (found == -1) return ERR_NOT_FOUND;

    char timestamp[MAX_TIME_LEN];
    now(timestamp, sizeof(timestamp));
    snprintf(lines[found], 128, "%s:%s:%s:%s\n",
             u.username, u.password_hash, STATUS_OFFLINE, timestamp);

    return save_lines(lines, count);
}

// permanently removes a user from users.txt
int deregister_user(const char *username) {
    char lines[MAX_USERS][128];
    int  count = load_users(lines);
    int  found = 0;
    User u;

    FILE *fp = fopen(USERS_FILE, "w");
    if (fp == NULL) return ERR_FILE;

    flock(fileno(fp), LOCK_EX);
    for (int i = 0; i < count; i++) {
        parse_line(lines[i], &u);
        if (strcasecmp(u.username, username) == 0) { found = 1; continue; }
        fputs(lines[i], fp);
    }
    flock(fileno(fp), LOCK_UN);

    fclose(fp);
    return found ? SUCCESS : ERR_NOT_FOUND;
}

// prints one user's status info into the terminal
int search_user(const char *username) {
    char lines[MAX_USERS][128];
    int  count = load_users(lines);
    User u;

    for (int i = 0; i < count; i++) {
        parse_line(lines[i], &u);
        if (strcasecmp(u.username, username) == 0) {
            printf("  -----------------------------------\n");
            printf("  | username  : %-16s│\n", u.username);
            printf("  | status    : %-16s│\n", u.status);
            printf("  | last seen : %-16s│\n", u.last_seen);
            printf("  ------------------------------------\n");
            return 1;
        }
    }
    return 0;
}

// lists all the users giving priority to those who are online
void list_users(void) {
    char lines[MAX_USERS][128];
    int  count = load_users(lines);
    User u;

    if (count == 0) { printf("  [*] no users registered yet.\n"); return; }

    printf("\n  ------------------------------------------------------\n");
    printf("  | #  | username          | status   | last seen        |\n");
    printf("  -----------------------------------------------------------\n");

    int row = 1;
    // ONLINE first 
    for (int i = 0; i < count; i++) {
        parse_line(lines[i], &u);
        if (strcmp(u.status, STATUS_ONLINE) == 0)
            printf("  │ %-2d │ %-17s │ \033[32m%-8s\033[0m │ %-16s │\n",
                    row++, u.username, u.status, u.last_seen);
    }
    // then OFFLINE 
    for (int i = 0; i < count; i++) {
        parse_line(lines[i], &u);
        if (strcmp(u.status, STATUS_OFFLINE) == 0)
            printf("  │ %-2d │ %-17s │ %-8s │ %-16s │\n",
                    row++, u.username, u.status, u.last_seen);
    }
    printf("  --------------------------------------------------------\n");
}

void build_user_list(char *buf, int buf_size) {
    char lines[MAX_USERS][128];
    int  count = load_users(lines);
    User u;

    snprintf(buf, buf_size, "%s:", CMD_LIST_RESULT);

    for (int i = 0; i < count; i++) {
        parse_line(lines[i], &u);
        
        strncat(buf, u.username, buf_size - strlen(buf) - 1);
        strncat(buf, ":",        buf_size - strlen(buf) - 1);
        strncat(buf, u.status,   buf_size - strlen(buf) - 1);
        strncat(buf, "|",        buf_size - strlen(buf) - 1);
    }
}