/*
 * SCS3304 — One-on-One Chat Application
 * User Manager Module Header
 *
 * Declares the User struct and all user management functions:
 * registration, login, logout, search, list, and deregistration.
 * All user data is persisted to data/users.txt — no database used.
 *
 * File format (one line per user):
 *   username:password_hash:status:last_seen
 */

#ifndef USER_MANAGER_H
#define USER_MANAGER_H

#define MAX_USERS        50
#define MAX_NAME_LEN     49
#define MAX_PASS_LEN     15    // max length of hash string 
#define MAX_TIME_LEN     20

#define USERS_FILE       "data/users.txt"

#define STATUS_ONLINE    "ONLINE"
#define STATUS_OFFLINE   "OFFLINE"

// return codes
#define SUCCESS          0
#define ERR_DUPLICATE   -1
#define ERR_NOT_FOUND   -2
#define ERR_WRONG_PASS  -3
#define ERR_ALREADY_ON  -4
#define ERR_FILE        -5
#define ERR_FULL        -6
#define ERR_INVALID     -7

typedef struct {
    char username[MAX_NAME_LEN + 1];
    char password_hash[MAX_PASS_LEN + 1];
    char status[10];
    char last_seen[MAX_TIME_LEN];
} User;


int  register_user(const char *username, const char *password);
int  login_user(const char *username, const char *password);
int  logout_user(const char *username);
int  deregister_user(const char *username);
int  search_user(const char *username);
void list_users(void);
int  user_exists(const char *username);
int  is_online(const char *username);

#endif