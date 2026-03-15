/*
 * SCS3304 — One-on-One Chat Application
 * Message Handler Module Header
 *
 * Declares functions for sending, storing, and displaying messages.
 *
 * File formats:
 *   messages.txt  — timestamp|from|to|body     (inbox store)
 *   chat_log.txt  — [timestamp] from → to : body  (audit trail)
 */

#ifndef MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H

#define MESSAGES_FILE  "data/messages.txt"
#define LOG_FILE       "data/chat_log.txt"
#define MAX_BODY_LEN   1024

/*
 * send_message  — store a message from one user to another
 * show_inbox    — display all messages received by a user
 * show_conversation — display message thread between two users
 */
int  send_message(const char *from, const char *to, const char *body);
void show_inbox(const char *username);
void show_conversation(const char *user_a, const char *user_b);

#endif