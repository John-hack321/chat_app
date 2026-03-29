
#ifndef MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H 

#define MESSAGES_FILE  "data/messages.txt"
#define LOG_FILE       "data/chat_log.txt"
#define MAX_BODY_LEN   1024


// store_message   — write a message to both files (with flock)
// show_inbox      — print all messages received by username
// show_history    — print thread between two users
// build_inbox_str — build inbox as a string for sending over socket

int  store_message(const char *from, const char *to, const char *body);
void show_inbox(const char *username);
void show_history(const char *user_a, const char *user_b);
void build_inbox_str(const char *username, char *buf, int buf_size);
void build_recent_str(const char *user_a, const char *user_b, int limit, char *buf, int buf_size);
void build_inbox_senders(const char *username, char *buf, int buf_size);

#endif