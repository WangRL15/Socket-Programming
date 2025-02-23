# Multi-Client Chat Room

A robust chat room application implemented in C using socket programming, featuring multi-client support, connection management, and automatic reconnection capabilities.

## Features

### Multi-Client Support
- Handles multiple client connections simultaneously
- Enables real-time communication between connected users
- Efficient message broadcasting system

### Multi-Process/Thread Implementation
- Utilizes parallel processing for handling multiple connections
- Ensures smooth operation without blocking other clients
- Optimized resource allocation for concurrent connections

### Connection Management

#### Timeout Handling
- Implements automatic timeout detection for inactive connections
- Prevents resource exhaustion from idle clients
- Configurable timeout duration
- Automatic cleanup of stale connections

#### Disconnection & Reconnection
- Graceful handling of unexpected disconnections
- Automatic reconnection mechanism for dropped clients
- Session persistence during reconnection attempts
- No need to restart client application after connection loss

## Technical Details

### Implementation
- Written in C programming language
- Implements TCP/IP protocol for reliable communication

### System Requirements

## Installation

1. Clone the repository

2. Compile the server:
```bash
gcc server.c -o server
```

3. Compile the client:
```bash
gcc client.c -o client
```

## Usage

### Starting the Server
```bash
./server [port]
```

### Connecting a Client
```bash
./client [server-ip] [port]
```

## Program Architecture

### Server
- Listens for incoming connections
- Manages client list
- Handles message broadcasting
- Monitors connection timeouts
- Manages resource cleanup

### Client
- Connects to server
- Handles user input
- Manages automatic reconnection
- Maintains connection status
