# chat-app

```
group members : 
    JOHN OKELLO     SCS3/2286/2023
    BRIAN KINOTI
```

A one-on-one chat application built in C, developed as part of a network and distributed programming course. The project is being built incrementally starting from a monolithic architecture with socket communication on a single machine, then later refactored into a full client-server model that can run across two different machines just as planned.

> **Current status:** Step 1 — user registration and file storage mechanism is implemented. Users can register a username which gets saved to a flat file (`data/users.txt`). Socket communication, messaging, and the full chat loop are not yet implemented.
this ensures we use the file mechanism as reqeusted by ms Ronge
---

## Project Structure

```
chat-app/
├── README.md
├── Makefile
├── docs/
│   ├── diagrams/          # architecture and function tree diagrams
│   ├── design.md          # architectural and algorithmic write-up
│   └── protocol.md        # application protocol documentation
├── src/
│   ├── main.c             # entry point
│   ├── server/
│   │   ├── server.c
│   │   └── server.h
│   ├── client/
│   │   ├── client.c
│   │   └── client.h
│   └── common/
│       ├── protocol.h         # message format constants
│       ├── utils.c / utils.h  # framing, parsing, timestamps
│       ├── user_manager.c     # register, deregister, search  ← implemented
│       ├── user_manager.h     # ← implemented
│       ├── message_handler.c  # logging, inbox
│       └── message_handler.h
└── data/
    ├── users.txt          # registered users (created at runtime)
    ├── messages.txt       # message store (created at runtime)
    └── chat_log.txt       # audit log (created at runtime)
```

---

## Requirements

Before you can build and run this project, you need the following installed on your machine.

=> I use linux , so a bit restructuring might be needed for windows specific devices

### Linux (Debian / Ubuntu / Kali)

**GCC — the C compiler**
```bash
sudo apt update
sudo apt install build-essential
```

`build-essential` installs `gcc`, `g++`, `make`, and other essential compilation tools in one command. Verify it worked:
```bash
gcc --version
```

You should see something like `gcc (Debian 12.2.0) 12.2.0` or similar. Any version above 9 is fine.

**Git — to clone the repo**
```bash
sudo apt install git
```

### macOS

Install the Xcode Command Line Tools — this gives you `gcc` (actually Apple's clang, which is compatible), `make`, and `git` all at once:
```bash
xcode-select --install
```

Verify:
```bash
gcc --version
make --version
```

### Windows

The easiest path on Windows is to use **WSL (Windows Subsystem for Linux)** and follow the Linux instructions above inside it. Alternatively install **MinGW-w64** for a native Windows gcc, but WSL is strongly recommended for this kind of systems programming.

---

## Getting Started

### 1. Fork and clone the repo

Click **Fork** on the GitHub page, then clone your fork:
```bash
git clone https://github.com/John-hack321/chat_app
cd chat-app
```

### 2. Create the data directory files

The `data/` folder needs to exist before running the program. If the files are not there yet, create them:
```bash
touch data/users.txt data/messages.txt data/chat_log.txt
```

### 3. Build

From the root of the project, run:
```bash
make
```

This compiles all the source files and produces a single binary called `chat` in the project root.

If you want to recompile from scratch:
```bash
make clean
make
```

### 4. Run

```bash
./chat
```

a simple terminal menu will appear on running the app:
NOTE: in this project we us makefile to controll which files are compiled by the gcc compiler at a time
```
=================================
   one-on-one chat — step 1
=================================

  1. register a user
  2. list all users
  3. quit

  choice:
```

Try registering a few usernames, listing them, and then trying to register the same name twice — it will tell you the username is already taken. Check the actual file that gets written:
```bash
cat data/users.txt
```

---

## What Is Implemented So Far

| Feature | Status |
|---|---|
| User registration | done |
| Duplicate username check | done |
| Save users to flat file (`users.txt`) | done |
| List all registered users | done |
| User deregistration | coming next |
| User search | coming next |
| Socket communication | coming next |
| Send / receive messages | coming next |
| Message inbox | coming next |
| Full monolith (server + client in one binary) | coming next |

---

## Architecture Overview

The application follows a **monolithic architecture** in this first iteration — one single compiled binary that contains both the server-side logic and the client-side logic, communicating internally over TCP on `127.0.0.1`. There is no separate server binary and no separate client binary yet.

The diagram below shows the intended full architecture for the monolith once all steps are complete:

```
main()
  ├── server_init()         (parent process — binds to port 8080)
  │     ├── socket_bind()
  │     ├── socket_listen()
  │     └── handle_client() → parse_command() → route_message()
  │
  └── client_init()         (child process — connects to 127.0.0.1:8080)
        ├── socket_connect()
        └── show_menu()
              ├── register_user()
              ├── send_message()
              ├── display_inbox()
              ├── search_user()
              └── deregister_user()

common/
  ├── frame_message()       solves TCP byte-stream framing
  ├── parse_message()       parses REG / MSG / SEARCH / DEREG / INBOX commands
  ├── log_message()         writes to messages.txt and chat_log.txt
  └── get_timestamp()

data/
  ├── users.txt             name:status  (e.g. alice:ONLINE)
  ├── messages.txt          timestamp|from|to|body
  └── chat_log.txt          append-only audit trail
```

After the monolith is complete, iteration 2 will split this into separate `server.c` and `client.c` binaries that can run on two different machines over a real network.

---

## Application Protocol

All messages sent over the socket follow this format (layer 5 — application protocol):
// I havent done this part yet but with colaboration we will get it all done soon

| Command | Format | Direction |
|---|---|---|
| Register | `REG:username` | client → server |
| Deregister | `DEREG:username` | client → server |
| Send message | `MSG:from:to:timestamp:body` | client → server |
| Search user | `SEARCH:username` | client → server |
| Read inbox | `INBOX:username` | client → server |
| Acknowledge | `ACK:OK` or `ACK:ERR:reason` | server → client |

Every message is prefixed with a 4-byte length header to handle TCP's byte-stream nature (messages can arrive fragmented — the length header tells the receiver exactly how many bytes to wait for).

---

## Development Notes

- No external libraries are used — only the C standard library and POSIX socket APIs
- No database — all persistence is done via plain text files in `data/`
- The code follows top-down analysis and modular programming principles as required by the course
- Tested on Kali Linux with gcc 12

---

## Course Context

This project is part of a networks and distributed programming course. The goals across all iterations are:

- **Iteration 1 (current):** monolithic app, single machine, sockets on localhost
- **Iteration 2:** client-server model, two separate binaries, two machines
- **Iteration 3:** group chat, broadcast server, multiple simultaneous clients