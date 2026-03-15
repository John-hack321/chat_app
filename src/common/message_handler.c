/*
 * SCS3304 — One-on-One Chat Application
 * Message Handler Module Implementation
 *
 * All messages are written to two files:
 *   messages.txt  — pipe-separated, used for inbox retrieval
 *   chat_log.txt  — human-readable audit trail, append-only
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "message_handler.h"
#include "user_manager.h"

static void now(char *buf, int size) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", tm_info);
}


int send_message(const char *from, const char *to, const char *body) {
    // validate the user first
    if (!user_exists(to)) return ERR_NOT_FOUND;

    char timestamp[24];
    now(timestamp, sizeof(timestamp));

    // we write to messages.txt
    FILE *fp = fopen(MESSAGES_FILE, "a");
    if (fp != NULL) {
        fprintf(fp, "%s|%s|%s|%s\n", timestamp, from, to, body);
        fclose(fp);
    }

    fp = fopen(LOG_FILE, "a");
    if (fp != NULL) {
        fprintf(fp, "[%s] %s -> %s : %s\n", timestamp, from, to, body);
        fclose(fp);
    }

    return SUCCESS;
}

// show weather a user has received any messages or not
void show_inbox(const char *username) {
    FILE *fp = fopen(MESSAGES_FILE, "r");
    if (fp == NULL) {
        printf("  [*] inbox is empty.\n");
        return;
    }

    char line[MAX_BODY_LEN + 128];
    char ts[24], from[50], to[50], body[MAX_BODY_LEN];
    int  count = 0;

    printf("\n  ┌──────────────────────────────────────────────────────┐\n");
    printf("  │  inbox for: %-40s│\n", username);
    printf("  ├──────────────────────────────────────────────────────┤\n");

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "%23[^|]|%49[^|]|%49[^|]|%1023[^\n]",
                    ts, from, to, body) == 4) {
            if (strcasecmp(to, username) == 0) {
                printf("  │ from : %-20s  at %s\n", from, ts);
                printf("  │   %s\n", body);
                printf("  ├──────────────────────────────────────────────────────┤\n");
                count++;
            }
        }
    }
    fclose(fp);

    if (count == 0)
        printf("  │  no messages yet.                                    │\n");

    printf("  │  %d message(s)                                         \n", count);
    printf("  └──────────────────────────────────────────────────────┘\n");
}


void show_conversation(const char *user_a, const char *user_b) {
    FILE *fp = fopen(MESSAGES_FILE, "r");
    if (fp == NULL) {
        printf("  [*] no conversation history yet.\n");
        return;
    }

    char line[MAX_BODY_LEN + 128];
    char ts[24], from[50], to[50], body[MAX_BODY_LEN];
    int  count = 0;

    printf("\n  ┌──────────────────────────────────────────────────────┐\n");
    printf("  │  conversation: %-16s ↔ %-14s│\n", user_a, user_b);
    printf("  ├──────────────────────────────────────────────────────┤\n");

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "%23[^|]|%49[^|]|%49[^|]|%1023[^\n]",
                    ts, from, to, body) == 4) {
            // include messages in either direction between the two users 
            int a_to_b = (strcasecmp(from, user_a) == 0 && strcasecmp(to, user_b) == 0);
            int b_to_a = (strcasecmp(from, user_b) == 0 && strcasecmp(to, user_a) == 0);
            if (a_to_b || b_to_a) {
                printf("  │ [%s] %s:\n", ts, from);
                printf("  │   %s\n", body);
                printf("  │\n");
                count++;
            }
        }
    }
    fclose(fp);

    if (count == 0)
        printf("  │  no messages exchanged yet.\n");

    printf("  └──────────────────────────────────────────────────────┘\n");
}