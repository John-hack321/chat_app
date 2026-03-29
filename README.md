# One-on-One Chat Application — SCS3304

```
Group members:
    JOHN OKELLO           SCS3/2286/2023
    BRIAN KINOTI          SCS3/146781/2023
    CHEBII NEHEMIAH KOECH SCS3/145415/2022
    MANDELA MBELEZI       P15/2108/2021
    ZACHARIAH ABDI        SCS3/147352/2023
```

A terminal-based one-on-one chat application written in C.
Built as Assignment 1 for the Networks and Distributed Programming course (SCS3304).

This is **iteration 2 — client-server model, single machine**.
The server and client run as separate binaries communicating over TCP on `127.0.0.1:8080`.
Two users can chat in real time from two different terminal windows on the same machine.

> Iteration 1 (standalone non client-server) is on the `main` branch.

---

## How It Works

```
terminal 1          terminal 2          terminal 3
./server            ./client            ./client
    │                   │                   │
    │←── connect() ─────┤                   │
    │←── connect() ─────────────────────────┤
    │                   │                   │
    │←── LOGIN:alice ───┤                   │
    │←── LOGIN:bob ─────────────────────────┤
    │                   │                   │
    │←── MSG:alice:bob:hello ───────────────┤  alice types
    │─── DELIVER:alice:hello ───────────────►  bob sees it instantly
```

Messages arrive in real time — no refreshing, no leaving the chat.

---

## Architecture

```
server binary (./server)              client binary (./client)
─────────────────────────             ──────────────────────────
server_run()                          client_run()
  └── select() loop                     └── welcome_menu()
        ├── accept() new clients               ├── screen_login()
        ├── recv_msg() per client              ├── screen_register()
        └── handle_command()                   └── logged_in_menu()
              ├── register_user()                    ├── screen_inbox()
              ├── login_user()                       ├── screen_start_chat()
              ├── store_message()                    └── chat_loop()
              └── find_client()                            └── select()
                    └── send_msg() ──► DELIVER                   ├── recv incoming
                                                                 └── send outgoing
```

---

## Concurrency Design

**Server — I/O multiplexing via `select()`**

The server runs as a single process and uses `select()` to watch all connected client
sockets simultaneously. When any socket has data, `select()` returns and the server
handles that client, then immediately goes back to watching all sockets again.

for us all the file descriptors live in the same process memory.
when alice sends to bob , the server finds bob's fd in `clients[]` and 
writes directly to it , we thus dont do cross-process type of communication

**Client — I/O multiplexing via `select()`**

The client's chat loop also uses `select()` to watch two things at once:
- `stdin` — the user might type a message
- the socket — the other user might send a message

Whichever becomes ready first gets handled. This is why incoming messages appear
instantly without blocking the user from typing.

**File locking via `flock()`**

Every write to `data/users.txt` and `data/messages.txt` is protected with `flock()`.
This prevents data corruption if two clients write simultaneously.

---

## Project Structure

```
chat-app/
├── README.md
├── Makefile                       # builds ./server and ./client
├── docs/
│   ├── diagrams/
│   ├── design.md
│   └── protocol.md
├── src/
│   ├── server_main.c              # entry point → ./server binary
│   ├── client_main.c              # entry point → ./client binary
│   ├── server/
│   │   ├── server.c               # select() loop, handle_command(), find_client()
│   │   └── server.h
│   ├── client/
│   │   ├── client.c               # menus, chat_loop() with select()
│   │   └── client.h
│   └── common/
│       ├── protocol.h             # command constants, port, buffer sizes
│       ├── utils.c / utils.h      # send_msg(), recv_msg() — TCP framing
│       ├── auth.c / auth.h        # djb2 password hashing
│       ├── user_manager.c / .h    # register, login, logout, search, list
│       └── message_handler.c / .h # store, inbox, history, recent messages
└── data/
    ├── users.txt                  # username:hash:status:last_seen
    ├── messages.txt               # timestamp|from|to|body
    └── chat_log.txt               # [timestamp] from -> to : body
```

---

## Application Protocol (Layer 5)

Every message sent over the TCP socket uses this format:

```
[4-byte length header][COMMAND:arg1:arg2:...]
```

The 4-byte length header solves TCP's framing problem — TCP is a byte stream with no
message boundaries. The receiver reads the length first, then reads exactly that many
bytes to get one complete message.

| Command | Format | Direction |
|---|---|---|
| Register | `REG:username:password` | client → server |
| Login | `LOGIN:username:password` | client → server |
| Logout | `LOGOUT:username` | client → server |
| Deregister | `DEREG:username` | client → server |
| List users | `LIST` | client → server |
| Search user | `SEARCH:username` | client → server |
| Send message | `MSG:from:to:body` | client → server |
| Get inbox | `INBOX:username` | client → server |
| Get recent | `RECENT:user_a:user_b` | client → server |
| Get senders | `SENDERS:username` | client → server |
| Acknowledge | `ACK:OK` or `ACK:ERR:reason` | server → client |
| Deliver message | `DELIVER:from:body` | server → client |
| List result | `LIST_RESULT:name:status\|...` | server → client |

---

## Data File Formats

**data/users.txt** — one line per registered user:
```
alice:193548712:OFFLINE:2024-03-14 09:00
bob:284719234:ONLINE:2024-03-14 10:23
```

**data/messages.txt** — one line per message:
```
2024-03-14 10:23:01|alice|bob|hey bob, are you there?
2024-03-14 10:24:15|bob|alice|yes! what's up?
```

**data/chat_log.txt** — append-only audit trail:
```
[2024-03-14 10:23:01] alice -> bob : hey bob, are you there?
[2024-03-14 10:24:15] bob -> alice : yes! what's up?
```

---

## Password Security

Passwords are hashed using the **djb2 algorithm** before being stored.

```
hash = 5381
for each character c:  hash = hash * 33 + c
```

---

## Requirements

### Linux (Debian / Ubuntu / Kali)

```bash
sudo apt update && sudo apt install build-essential git
gcc --version   # versions above 9
```

### macOS

```bash
xcode-select --install
```

### Windows

Use WSL and follow the Linux steps.

---

## Getting Started

```bash
# 1. switch to the client-server branch
git checkout client-server

# 2. create data files (only needed once)
touch data/users.txt data/messages.txt data/chat_log.txt

# 3. build both binaries
make

# 4. terminal 1 — start server first
./server

# 5. terminal 2 — first user
./client

# 6. terminal 3 (optional) — second user for live chat test
./client
```

To rebuild from scratch:
```bash
make clean && make
```

---

## Usage Walkthrough

**Terminal 1 (server):**
```
  [*] server listening on 127.0.0.1:8080
  [*] concurrency: select() I/O multiplexing
  [*] press Ctrl+C to stop

  [+] new client (fd=4) — 1 connected
  [+] alice logged in
  [+] new client (fd=5) — 2 connected
  [+] bob logged in
```

**Terminal 2 (alice):**
```
  ╔══════════════════════════════════════════╗
  ║  chat: alice           ↔  bob           ║
  ║  /quit to leave                          ║
  ╚══════════════════════════════════════════╝

  you: hey bob!
  you:
```

**Terminal 3 (bob) — message appears instantly:**
```
  bob      : hey bob!
  you:
```

---

## Current Status

| Feature | Status |
|---|---|
| User registration with password | done |
| Login / logout | done |
| ONLINE / OFFLINE status | done |
| Real-time message delivery | done |
| Conversation history (last 8 messages) | done |
| Inbox with sender list | done |
| Search user | done |
| List all users | done |
| Delete account | done |
| TCP framing (length-prefixed messages) | done |
| select() server concurrency | done |
| flock() file locking | done |

---

## Module Descriptions

| Module | File | Responsibility |
|---|---|---|
| Server entry point | `src/server_main.c` | starts `./server` |
| Client entry point | `src/client_main.c` | starts `./client` |
| Server logic | `src/server/server.c` | select() loop, command routing, real-time delivery |
| Client logic | `src/client/client.c` | menus, chat loop with select() |
| TCP framing | `src/common/utils.c` | send_msg() / recv_msg() with 4-byte length header |
| Authentication | `src/common/auth.c` | djb2 hashing, password verification |
| User management | `src/common/user_manager.c` | register, login, logout, flock() writes |
| Message handling | `src/common/message_handler.c` | store, inbox, history, recent |

---

## Iterations

| Iteration | Branch | Description | Status |
|---|---|---|---|
| 1 | `main` | Standalone, single machine, no sockets | complete |
| 2 | `client-server` | Client-server, TCP sockets, single machine | current |
| 3 | upcoming | Group chat, broadcast server | upcoming |