/*
 * SCS3304 — One-on-One Chat Application
 * User Manager Module Implementation
 *
 * Handles all user lifecycle operations. Every function that modifies
 * the user list follows the same pattern:
 *   1. Read all lines from users.txt into a buffer
 *   2. Modify the relevant line in memory
 *   3. Write all lines back to disk
 *
 * This approach keeps the file always consistent and avoids partial writes.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "user_manager.h"
#include "auth.h"

static void now(char *buf, int size) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(buf, size, "%Y-%m-%d %H:%M", tm_info);
}

// internal: parse one line from users.txt into a User struct 
static void parse_line(const char *line, User *u) {
    sscanf(line, "%49[^:]:%15[^:]:%9[^:]:%19[^\n]",
        u->username, u->password_hash, u->status, u->last_seen);
}

// internal: read entire users.txt into an array, return line count 
static int load_users(char lines[MAX_USERS][128]) {
    FILE *fp = fopen(USERS_FILE, "r");
    if (fp == NULL) return 0;
    int count = 0;
    while (count < MAX_USERS && fgets(lines[count], 128, fp) != NULL)
        count++;
    fclose(fp);
    return count;
}

// internal: write all lines back to users.txt 
static int save_lines(char lines[MAX_USERS][128], int count) {
    FILE *fp = fopen(USERS_FILE, "w");
    if (fp == NULL) return ERR_FILE;
    for (int i = 0; i < count; i++)
        fputs(lines[i], fp);
    fclose(fp);
    return SUCCESS;
}

/* 
 * FUNCTION : user_exists
 * PURPOSE  : Check whether a username is registered
 * INPUT    : username — string to look up
 * OUTPUT   : 1 if found, 0 if not found
 */
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

/* 
 * FUNCTION : is_online
 * PURPOSE  : Check whether a registered user is currently ONLINE
 * INPUT    : username — string to look up
 * OUTPUT   : 1 if ONLINE, 0 otherwise
 */
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

/* 
 * FUNCTION : register_user
 * PURPOSE  : Add a new user to users.txt with a hashed password
 * INPUT    : username — desired login handle
 *            password — plain-text password (will be hashed)
 * OUTPUT   : SUCCESS, ERR_DUPLICATE, ERR_FULL, ERR_INVALID, ERR_FILE
 * STEPS    : 1. Validate input lengths
 *            2. Check for duplicate username
 *            3. Hash the password
 *            4. Append new line to users.txt
 */
int register_user(const char *username, const char *password) {
    /* --- VALIDATE --- */
    if (strlen(username) == 0)               return ERR_INVALID;
    if (strlen(password) < MIN_PASSWORD_LEN) return ERR_INVALID;

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

    FILE *fp = fopen(USERS_FILE, "a");
    if (fp == NULL) return ERR_FILE;
    fprintf(fp, "%s:%s:%s:%s\n", username, hash_str, STATUS_OFFLINE, timestamp);
    fclose(fp);

    return SUCCESS;
}

/* 
 * FUNCTION : login_user
 * PURPOSE  : Verify credentials and mark the user as ONLINE
 * INPUT    : username, password — credentials entered by user
 * OUTPUT   : SUCCESS, ERR_NOT_FOUND, ERR_WRONG_PASS, ERR_ALREADY_ON
 * STEPS    : 1. Find user line in file
 *            2. Verify password hash
 *            3. Rewrite that line with ONLINE status + new timestamp
 */
int login_user(const char *username, const char *password) {
    char lines[MAX_USERS][128];
    int  count = load_users(lines);
    int  found = -1;
    User u;

    /* --- VALIDATE --- */
    for (int i = 0; i < count; i++) {
        parse_line(lines[i], &u);
        if (strcasecmp(u.username, username) == 0) { found = i; break; }
    }
    if (found == -1)                          return ERR_NOT_FOUND;
    if (!verify_password(password, u.password_hash)) return ERR_WRONG_PASS;
    if (strcmp(u.status, STATUS_ONLINE) == 0) return ERR_ALREADY_ON;

    /* --- PROCESS --- */
    char timestamp[MAX_TIME_LEN];
    now(timestamp, sizeof(timestamp));
    snprintf(lines[found], 128, "%s:%s:%s:%s\n",
            u.username, u.password_hash, STATUS_ONLINE, timestamp);

    return save_lines(lines, count);
}

/* 
 * FUNCTION : logout_user
 * PURPOSE  : Mark the user as OFFLINE and record the time
 * INPUT    : username — currently logged-in user
 * OUTPUT   : SUCCESS, ERR_NOT_FOUND, ERR_FILE
 * */
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

/* 
* 1. Load all lines
* 2. Write back every line EXCEPT the target user
 **/
int deregister_user(const char *username) {
    char lines[MAX_USERS][128];
    int  count = load_users(lines);
    int  found = 0;
    User u;

    FILE *fp = fopen(USERS_FILE, "w");
    if (fp == NULL) return ERR_FILE;

    for (int i = 0; i < count; i++) {
        parse_line(lines[i], &u);
        if (strcasecmp(u.username, username) == 0) {
            found = 1;
            continue;   /* skip this line — effectively deletes the user */
        }
        fputs(lines[i], fp);
    }
    fclose(fp);

    return found ? SUCCESS : ERR_NOT_FOUND;
}

/* 
 * : search_user
 *  : Print status info for a specific user
 *    : username — user to look up
 *   : 1 if found and printed, 0 if not found
 * */
int search_user(const char *username) {
    char lines[MAX_USERS][128];
    int  count = load_users(lines);
    User u;

    for (int i = 0; i < count; i++) {
        parse_line(lines[i], &u);
        if (strcasecmp(u.username, username) == 0) {
            printf("  ┌─────────────────────────────┐\n");
            printf("  │ username  : %-16s│\n", u.username);
            printf("  │ status    : %-16s│\n", u.status);
            printf("  │ last seen : %-16s│\n", u.last_seen);
            printf("  └─────────────────────────────┘\n");
            return 1;
        }
    }
    return 0;
}

/* 
 *  : list_users
 *  : Print a formatted table of all registered users
 *            Sorted so ONLINE users appear first
 * */
void list_users(void) {
    char lines[MAX_USERS][128];
    int  count = load_users(lines);
    User u;

    if (count == 0) {
        printf("  [*] no users registered yet.\n");
        return;
    }

    // printing with good a header for better ux 
    printf("\n  ┌────┬───────────────────┬──────────┬──────────────────┐\n");
    printf("  │ #  │ username          │ status   │ last seen        │\n");
    printf("  ├────┼───────────────────┼──────────┼──────────────────┤\n");

    int row = 1;
    for (int i = 0; i < count; i++) {
        parse_line(lines[i], &u);
        if (strcmp(u.status, STATUS_ONLINE) == 0)
            printf("  │ %-2d │ %-17s │ \033[32m%-8s\033[0m │ %-16s │\n",
                row++, u.username, u.status, u.last_seen);
    }

    for (int i = 0; i < count; i++) {
        parse_line(lines[i], &u);
        if (strcmp(u.status, STATUS_OFFLINE) == 0)
            printf("  │ %-2d │ %-17s │ %-8s │ %-16s │\n",
                row++, u.username, u.status, u.last_seen);
    }

    printf("  └────┴───────────────────┴──────────┴──────────────────┘\n");
}