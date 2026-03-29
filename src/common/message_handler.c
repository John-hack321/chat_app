#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/file.h>
#include "message_handler.h"
#include "user_manager.h"

static void now(char *buf, int size) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

// stores the message by persistingit ot the message.txt file and chat_log.txt files
int store_message(const char *from, const char *to, const char *body) {
    if (!user_exists(to)) return ERR_NOT_FOUND;

    char timestamp[24];
    now(timestamp, sizeof(timestamp));

    // write to messages.txt — lock while writing 
    FILE *fp = fopen(MESSAGES_FILE, "a");
    if (fp != NULL) {
        flock(fileno(fp), LOCK_EX);
        fprintf(fp, "%s|%s|%s|%s\n", timestamp, from, to, body);
        flock(fileno(fp), LOCK_UN);
        fclose(fp);
    }

    // write to chat_log.txt — separate lock 
    fp = fopen(LOG_FILE, "a");
    if (fp != NULL) {
        flock(fileno(fp), LOCK_EX);
        fprintf(fp, "[%s] %s -> %s : %s\n", timestamp, from, to, body);
        flock(fileno(fp), LOCK_UN);
        fclose(fp);
    }

    return SUCCESS;
}

// show the inbox so the user can see pendig messages
void show_inbox(const char *username) {
    FILE *fp = fopen(MESSAGES_FILE, "r");
    if (fp == NULL) { printf("  [*] inbox is empty.\n"); return; }

    flock(fileno(fp), LOCK_SH);

    char line[MAX_BODY_LEN + 128];
    char ts[24], from[50], to[50], body[MAX_BODY_LEN];
    int  count = 0;

    printf("\n  ------------------------------------------------------\n");
    printf("  │  inbox for: %-40s│\n", username);
    printf("  ----------------------------------------------------------\n");

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "%23[^|]|%49[^|]|%49[^|]|%1023[^\n]",
                   ts, from, to, body) == 4) {
            if (strcasecmp(to, username) == 0) {
                printf("   from: %-15s   at %s\n", from, ts);
                printf("     %s\n", body);
                printf("  -----------------------------------------------\n");
                count++;
            }
        }
    }
    if (count == 0)
        printf("  | no messages yet.                                    │\n");
    printf("  │  %d message(s)                                        \n", count);
    printf("  -----------------------------------------------------------------\n");

    flock(fileno(fp), LOCK_UN);
    fclose(fp);
}

// prints the message of all messages sent between two users 
void show_history(const char *user_a, const char *user_b) {
    FILE *fp = fopen(MESSAGES_FILE, "r");
    if (fp == NULL) { printf("  [*] no history yet.\n"); return; }

    flock(fileno(fp), LOCK_SH);

    char line[MAX_BODY_LEN + 128];
    char ts[24], from[50], to[50], body[MAX_BODY_LEN];
    int  count = 0;

    printf("\n  ------------------------------------------------------\n");
    printf("  |  %s  ↔  %-38s│\n", user_a, user_b);
    printf("  ---------------------------------------------------------\n");

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "%23[^|]|%49[^|]|%49[^|]|%1023[^\n]",
                   ts, from, to, body) == 4) {
            int a_b = strcasecmp(from,user_a)==0 && strcasecmp(to,user_b)==0;
            int b_a = strcasecmp(from,user_b)==0 && strcasecmp(to,user_a)==0;
            if (a_b || b_a) {
                printf("  │ [%s] %s:\n  │   %s\n  │\n", ts, from, body);
                count++;
            }
        }
    }
    if (count == 0)
        printf("  |  no messages exchanged yet.\n");
    printf("  ---------------------------------------------\n");

    flock(fileno(fp), LOCK_UN);
    fclose(fp);
}

void build_inbox_str(const char *username, char *buf, int buf_size) {
    FILE *fp = fopen(MESSAGES_FILE, "r");
    snprintf(buf, buf_size, "INBOX_RESULT:");
    if (fp == NULL) return;

    flock(fileno(fp), LOCK_SH);

    char line[MAX_BODY_LEN + 128];
    char ts[24], from[50], to[50], body[MAX_BODY_LEN];

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "%23[^|]|%49[^|]|%49[^|]|%1023[^\n]",
                   ts, from, to, body) == 4) {
            if (strcasecmp(to, username) == 0) {
                char entry[MAX_BODY_LEN + 128];
                snprintf(entry, sizeof(entry), "[%s] %s: %s~~", ts, from, body);
                strncat(buf, entry, buf_size - strlen(buf) - 1);
            }
        }
    }

    flock(fileno(fp), LOCK_UN);
    fclose(fp);
}

// returns the last limit messages exchanged between two users as a single for sending ober the socket communication.
//   Output format:
//     RECENT_RESULT:sender|body~~sender|body~~...
//   Oldest message first so the client can print top to bottom.
void build_recent_str(const char *user_a, const char *user_b,
                      int limit, char *buf, int buf_size) {
    snprintf(buf, buf_size, "RECENT_RESULT:");

    FILE *fp = fopen(MESSAGES_FILE, "r");
    if (fp == NULL) return;

    flock(fileno(fp), LOCK_SH);

    // store matching lines in a circular buffer of `limit` slots 
    char senders[20][50];
    char bodies[20][MAX_BODY_LEN];
    int  total = 0;

    char line[MAX_BODY_LEN + 128];
    char ts[24], from[50], to[50], body[MAX_BODY_LEN];

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "%23[^|]|%49[^|]|%49[^|]|%1023[^\n]",
                    ts, from, to, body) == 4) {
            int a_b = strcasecmp(from,user_a)==0 && strcasecmp(to,user_b)==0;
            int b_a = strcasecmp(from,user_b)==0 && strcasecmp(to,user_a)==0;
            if (a_b || b_a) {
                // circular slot — overwrites oldest when full 
                int slot = total % limit;
                strncpy(senders[slot], from,  49);
                strncpy(bodies[slot],  body, MAX_BODY_LEN - 1);
                senders[slot][49] = '\0';
                bodies[slot][MAX_BODY_LEN - 1] = '\0';
                total++;
            }
        }
    }

    flock(fileno(fp), LOCK_UN);
    fclose(fp);

    if (total == 0) return;


    // The circular buffer holds the last `limit` messages.
    // Figure out the start index and count to iterate in order.

    int count = total < limit ? total : limit;
    int start = total < limit ? 0 : (total % limit);

    for (int i = 0; i < count; i++) {
        int slot = (start + i) % limit;
        char entry[MAX_BODY_LEN + 64];
        snprintf(entry, sizeof(entry), "%s|%s~~", senders[slot], bodies[slot]);
        strncat(buf, entry, buf_size - strlen(buf) - 1);
    }
}

//Build a deduplicated list of people who have sent
// messages to `username`, most recent sender first.
// this allows the user to see all of people who have sent a message

void build_inbox_senders(const char *username, char *buf, int buf_size) {
    snprintf(buf, buf_size, "SENDERS_RESULT:");

    FILE *fp = fopen(MESSAGES_FILE, "r");
    if (fp == NULL) return;

    flock(fileno(fp), LOCK_SH);

    char senders[MAX_BODY_LEN];   /* ordered list, newest last */
    char unique[50][50];
    int  ucount = 0;

    char line[MAX_BODY_LEN + 128];
    char ts[24], from[50], to[50], body[MAX_BODY_LEN];
    (void)senders;

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "%23[^|]|%49[^|]|%49[^|]|%1023[^\n]",
                    ts, from, to, body) == 4) {
            if (strcasecmp(to, username) == 0) {
                /* check if already in list */
                int found = 0;
                for (int i = 0; i < ucount; i++) {
                    if (strcasecmp(unique[i], from) == 0) {
                        found = 1;
                        /* move to end (most recent) */
                        memmove(&unique[i], &unique[i+1],
                                (ucount - i - 1) * sizeof(unique[0]));
                        strncpy(unique[ucount - 1], from, 49);
                        break;
                    }
                }
                if (!found && ucount < 50) {
                    strncpy(unique[ucount], from, 49);
                    unique[ucount][49] = '\0';
                    ucount++;
                }
            }
        }
    }

    flock(fileno(fp), LOCK_UN);
    fclose(fp);

    // build result string — reverse so newest is first 
    for (int i = ucount - 1; i >= 0; i--) {
        strncat(buf, unique[i], buf_size - strlen(buf) - 1);
        strncat(buf, "|",       buf_size - strlen(buf) - 1);
    }
}