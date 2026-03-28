# One-on-One Chat Application — SCS3304

```
Group members:
    JOHN OKELLO           SCS3/2286/2023
    BRIAN KINOTI          SCS3/146781/2023
    CHEBII NEHEMIAH KOECH SCS3/145415/2022
    MANDELA MBELEZI       P15/2108/2021
    ZACHARIAH ABDI        SCS3/147352/2023
```

A standalone terminal-based one-on-one chat application written in C.
Built as Assignment 1 for the Networks and Distributed Programming course (SCS3304).

This is **iteration 1 — standalone (non client-server)**. Two users share one machine.
Each user logs in, sends and reads messages, then logs out. The next user logs in and
sees their inbox. No sockets, no network layer — persistence is handled entirely through
flat text files as required by the course.

---

## Current Status

| Feature | Status  |
|---|---|
| User registration with password | done |
| Duplicate username check | done |
| Password hashing (djb2) | done |
| Login / logout | done |
| ONLINE / OFFLINE status tracking | done |
| Send message to registered user | done |
| View inbox (all received messages) | done |
| View conversation thread (both directions) | done |
| Search user by username | done |
| List all users with status | done |
| Delete account (deregister) | done |
| Flat file persistence (no database) | done |

---

## Project Structure

```
chat-app/
├── README.md
├── Makefile
├── docs/
│   ├── diagrams/              # architecture and function tree diagrams
│   ├── design.md              # architectural and algorithmic write-up
│   └── protocol.md            # protocol documentation (iteration 2)
├── src/
│   ├── main.c                 # entry point, all menu logic
│   ├── server/                # reserved for iteration 2 (client-server)
│   │   ├── server.c
│   │   └── server.h
│   ├── client/                # reserved for iteration 2 (client-server)
│   │   ├── client.c
│   │   └── client.h
│   └── common/
│       ├── auth.c / auth.h          # password hashing and verification
│       ├── user_manager.c / .h      # register, login, logout, search, list
│       └── message_handler.c / .h   # send, inbox, conversation history
└── data/
    ├── users.txt              # username:hash:status:last_seen
    ├── messages.txt           # timestamp|from|to|body
    └── chat_log.txt           # [timestamp] from -> to : body
```

---

## Architecture

```
main()
  └── welcome_menu()                     ← login / register / list / exit
        ├── do_register()                → register_user()
        │                                  auth: hash_password()
        ├── do_login()                   → login_user()
        │                                  auth: verify_password()
        └── logged_in_menu()             ← shown after successful login
              ├── do_send_message()      → send_message()   → messages.txt
              │                                             → chat_log.txt
              ├── do_inbox()             → show_inbox()     ← messages.txt
              ├── do_conversation()      → show_conversation()
              ├── do_search()            → search_user()    ← users.txt
              ├── do_logout()            → logout_user()    → users.txt
              └── do_deregister()        → deregister_user()
```

All user state lives in `data/users.txt`. All messages live in `data/messages.txt`.
The `data/chat_log.txt` file is an append-only audit trail — nothing is ever deleted from it.

---

## Data File Formats

**data/users.txt** — one line per registered user:
```
alice:193548712:OFFLINE:2024-03-14 09:00
bob:284719234:ONLINE:2024-03-14 10:23
```
Fields: `username : password_hash : status : last_seen`

**data/messages.txt** — one line per message sent:
```
2024-03-14 10:23:01|alice|bob|hey bob, are you there?
2024-03-14 10:24:15|bob|alice|yes! what's up?
```
Fields: `timestamp | from | to | body`

**data/chat_log.txt** — human-readable audit trail:
```
[2024-03-14 10:23:01] alice -> bob : hey bob, are you there?
[2024-03-14 10:24:15] bob -> alice : yes! what's up?
```

---

## Password Security

Passwords are never stored as plain text. When a user registers, the password is passed
through the **djb2 hash algorithm**:

```
hash = 5381
for each character c:  hash = hash * 33 + c
```

The resulting integer is stored as a string in `users.txt`. At login, the entered password
is hashed again and compared to the stored value. If the hashes match, login succeeds.

---

## Requirements

### Linux (Debian / Ubuntu / Kali)

```bash
sudo apt update && sudo apt install build-essential git
```

Verify:
```bash
gcc --version   # any version above 9 is fine
make --version
```

### macOS

```bash
xcode-select --install
```

### Windows

Use WSL (Windows Subsystem for Linux) and follow the Linux steps inside it.

---

## Getting Started

```bash
# 1. clone the repo
git clone https://github.com/John-hack321/chat_app
cd chat-app

# 2. create the data files (only needed once)
touch data/users.txt data/messages.txt data/chat_log.txt

# 3. build
make

# 4. run
./chat
```

To rebuild from scratch:
```bash
make clean && make
```

---

## Usage Walkthrough

```
╔══════════════════════════════════════════╗
║      one-on-one chat  —  SCS3304        ║
║      standalone (non client-server)     ║
╚══════════════════════════════════════════╝

  1.  login
  2.  register new account
  3.  list all users
  4.  exit
  choice:
```

**Typical flow:**
1. User A registers → sets username + password
2. User A logs in → sends a message to User B
3. User A logs out
4. User B logs in → sees message in inbox → replies
5. User B logs out
6. Repeat

---

## Module Descriptions

| Module | File | Responsibility |
|---|---|---|
| Entry point + menus | `src/main.c` | All user-facing screens and menu logic |
| Authentication | `src/common/auth.c` | djb2 password hashing and verification |
| User management | `src/common/user_manager.c` | Register, login, logout, search, list, deregister |
| Message handling | `src/common/message_handler.c` | Send, inbox, conversation history, file logging |

---

## Is Concurrency Required?

No. In the standalone model, only one user is active at a time. Users take turns —
there is no simultaneous access to shared data. Therefore no threads, no `select()`,
and no mutex locking is needed.

Concurrency **will** be required in iteration 2 (client-server), where multiple clients
connect simultaneously and the server must handle them without blocking.

---

## Development Notes

- No external libraries — only the C standard library
- No database — flat text files only, as required by Ms. Ronge
- All file operations check for `NULL` before use
- Top-down structured programming and modular design throughout
- Tested on Kali Linux with gcc 12

---

## Iterations

| Iteration | Description | Status |
|---|---|---|
| 1 | Standalone, single machine, no sockets | current |
| 2 | Client-server, two binaries, TCP sockets | upcoming |
| 3 | Group chat, broadcast, multiple clients | upcoming |