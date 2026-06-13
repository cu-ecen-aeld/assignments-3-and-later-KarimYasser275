# AESD Socket Server (`aesdsocket`)

A multi-threaded TCP socket server that listens on port **9000**, receives newline-delimited data from clients, persists it to a shared file, and echoes the full file contents back. Designed as part of the AESD (Advanced Embedded Software Development) coursework.

---

## Overview

`aesdsocket` implements a concurrent TCP server using POSIX threads. Each client connection is handled in a dedicated thread, and all received data is appended to a shared file at `/var/tmp/aesdsocketdata`. A separate background thread appends an RFC 2822–formatted timestamp to the same file every 10 seconds.

---

## Architecture

```
┌──────────────────────────────────────────────────────┐
│                     Main Thread                      │
│                                                      │
│   socket() → bind() → listen() → accept() loop      │
│       │                                │             │
│       │  spawns per-connection         │  joins      │
│       ▼                                ▼             │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐     │
│  │  Worker #1 │  │  Worker #2 │  │  Worker #N │     │
│  │  (recv →   │  │  (recv →   │  │  (recv →   │     │
│  │  write →   │  │  write →   │  │  write →   │     │
│  │  send)     │  │  send)     │  │  send)     │     │
│  └─────┬──────┘  └─────┬──────┘  └─────┬──────┘     │
│        │               │               │             │
│        └───────────┬────┘───────────────┘             │
│                    ▼                                  │
│        /var/tmp/aesdsocketdata  (shared file)         │
│                    ▲                                  │
│                    │                                  │
│           ┌────────┴────────┐                         │
│           │ Timestamp Thread│                         │
│           │ (every 10 sec)  │                         │
│           └─────────────────┘                         │
└──────────────────────────────────────────────────────┘
```

---

## Features

| Feature | Description |
|---|---|
| **TCP Server** | Listens on port 9000 using IPv4 (`PF_INET`) with `SO_REUSEADDR` |
| **Daemon Mode** | Optional `-d` flag forks to background via `daemon(0, 0)` |
| **Multi-threaded** | Each accepted connection spawns a POSIX thread for concurrent I/O |
| **Persistent Storage** | Received data is appended to `/var/tmp/aesdsocketdata` |
| **Echo-back** | After a complete newline-terminated packet is received, the entire file is sent back to the client |
| **Timestamps** | A dedicated thread appends an RFC 2822 timestamp every 10 seconds |
| **Graceful Shutdown** | Catches `SIGINT` / `SIGTERM`, cleans up all resources, and deletes the data file |
| **Syslog Logging** | Logs accepted and closed connections via `syslog` at `LOG_INFO` level |

---

## Command-Line Usage

```bash
# Run in foreground
./aesdsocket

# Run as a daemon (background)
./aesdsocket -d
```

### Options

| Flag | Description |
|---|---|
| `-d` | Run the server as a daemon process |

---

## Server State Machine

The main loop progresses through a state machine with the following states:

```
SOCKET_CREATE ──► SOCKET_BIND ──► LISTEN ──► ACCEPT ──► CHECK ──┐
                                    ▲                            │
                                    └────────────────────────────┘
```

| State | Action |
|---|---|
| `SOCKET_CREATE` | Creates the TCP socket, sets `SO_REUSEADDR`, and resolves the local address with `getaddrinfo` |
| `SOCKET_BIND` | Binds the socket to port 9000 on all interfaces |
| `LISTEN` | Starts listening with a backlog of 10 |
| `ACCEPT` | Blocks until a client connects; logs the client IP; opens the data file; spawns a worker thread; pushes the thread node onto the linked list |
| `CHECK` | Iterates the linked list of threads, joining and freeing any that have completed; then transitions back to `LISTEN` |

---

## Per-Connection Thread (`thread_ReceiveSend`)

Each worker thread follows its own sub-state machine:

```
RECEIVE ──► SEND ──► DONE
```

### RECEIVE State
1. Calls `recv()` in a loop, dynamically growing a `packet` buffer with `realloc`.
2. Scans the buffer for a newline character (`\n`).
3. Once a complete newline-terminated packet is assembled:
   - Acquires `thread_mutex`.
   - Appends the packet to `/var/tmp/aesdsocketdata` via `write()`.
   - Releases `thread_mutex`.
   - Transitions to **SEND**.

### SEND State
1. Calls `fstat()` to determine the current file size.
2. Re-opens `/var/tmp/aesdsocketdata` from the beginning and reads the entire contents.
3. Sends the full file contents back to the client byte-by-byte via `send()`.
4. Logs the closed connection via `syslog`.
5. Transitions to **DONE**.

### DONE State
1. Marks the thread node as `thread_completed = true`.
2. Frees allocated buffers and closes file/socket descriptors.
3. Returns (thread exits).

---

## Timestamp Thread (`thread_timeStamp`)

Runs as a background thread for the lifetime of the server:

1. Every **10 seconds**, formats the current time as an RFC 2822–style string:
   ```
   timestamp:Mon, 09 Jun 2026 14:30:00 +0300
   ```
2. Acquires `thread_mutex`, opens the data file in append mode, writes the timestamp, and closes/unlocks.

This ensures timestamps are interleaved with client data in the shared file.

---

## Synchronization

Two mutexes are used:

| Mutex | Purpose |
|---|---|
| `thread_mutex` | Serializes writes to `/var/tmp/aesdsocketdata` (used by both worker threads and the timestamp thread) |
| `ll_mutex` | Protects the linked list of thread nodes during the CHECK state |

---

## Thread Management (Linked List)

Active connections are tracked in a singly-linked list of `Node_t` structs:

```c
typedef struct Node_s {
    pthread_t t;              // Thread handle
    int fd;                   // Data file descriptor
    int acceptfd;             // Client socket descriptor
    bool thread_completed;    // Completion flag
    substate_t state;         // Current sub-state
    struct Node_s *next;      // Next node in list
} Node_t;
```

- New nodes are prepended to the list head on each `accept()`.
- The `CHECK` state walks the list, joining completed threads and freeing their nodes.

---

## Signal Handling & Graceful Exit

On receiving **SIGINT** or **SIGTERM**, the `gracefull_exit` handler:

1. Closes the server socket, data file, and last accepted socket.
2. Frees the `addrinfo` structure.
3. Deletes `/var/tmp/aesdsocketdata` via `unlink()`.
4. Logs `"Caught signal, exiting"` to syslog.
5. Closes syslog and calls `exit(EXIT_SUCCESS)`.

---

## Init Script (`aesdsocket-start-stop`)

A companion start/stop script is provided for integration with an embedded Linux init system:

```bash
# Start the server as a daemon
aesdsocket-start-stop start

# Stop the server (sends SIGTERM)
aesdsocket-start-stop stop
```

---

## File Layout

```
server/
├── aesdsocket.c              # Server source code
├── aesdsocket                 # Compiled binary
├── aesdsocket-start-stop      # Init script for start/stop
├── Makefile                   # Build rules
├── valgrind-out.txt           # Valgrind memory analysis output
└── AESD_SOCKET.md             # This documentation
```

---

## Build

```bash
cd server
make
```

---

## Data Flow Example

```
Client                        Server                      /var/tmp/aesdsocketdata
  │                             │                                   │
  │── "hello\n" ───────────────►│                                   │
  │                             │── write("hello\n") ──────────────►│
  │                             │◄── read entire file ──────────────│
  │◄── "hello\n" ──────────────│                                   │
  │                             │                                   │
  │                             │   (10 sec timer fires)            │
  │                             │── write("timestamp:...") ────────►│
  │                             │                                   │
  │── "world\n" ───────────────►│                                   │
  │                             │── write("world\n") ──────────────►│
  │                             │◄── read entire file ──────────────│
  │◄── "hello\ntimestamp:...\n  │                                   │
  │     world\n" ──────────────│                                   │
  │                             │                                   │
```
