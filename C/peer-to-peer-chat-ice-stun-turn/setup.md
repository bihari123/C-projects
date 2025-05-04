# P2P Communication App Setup Guide

This guide provides step-by-step instructions to build and run the P2P communication application with STUN/TURN support for NAT traversal.

## Prerequisites

The application requires the following dependencies:
- GLib 2.0 development libraries
- libnice (for ICE/STUN/TURN support)
- Compiler toolchain (gcc, make)

## Installation of Dependencies

### On Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install build-essential libnice-dev libglib2.0-dev
```

### On Fedora/RHEL/CentOS

```bash
sudo dnf install gcc make libnice-devel glib2-devel
```

### On macOS

Using Homebrew:
```bash
brew install glib libnice
```

## Building the Application

1. Clone the repository or extract the source code to a directory
2. Navigate to the project directory
3. Use make to build the application:

```bash
# Using the provided Makefile
make

# Alternatively, to install dependencies and then build (Debian/Ubuntu)
make install-deps
make
```

The build process will:
1. Compile all source files in the `src` directory
2. Link them together with the required libraries
3. Create the executable `p2p` in the project root directory

## Running the Application

The P2P communication app can be run in different modes depending on your needs:

### Basic Local Testing (No ICE)

For simple testing on the same machine:

```bash
# Terminal 1 (Server)
./p2p -s --no-ice

# Terminal 2 (Client)
./p2p -c 127.0.0.1 --no-ice
```

### Using STUN for NAT Traversal

For communication between peers on different networks:

```bash
# On machine A (Server)
./p2p -s --stun stun.l.google.com

# On machine B (Client)
./p2p --stun stun.l.google.com
```

### Using TURN Server (When Direct Connection Fails)

For difficult NAT configurations:

```bash
# On machine A (Server)
./p2p -s --turn turn.example.com --turn-user username --turn-pass password

# On machine B (Client)
./p2p --turn turn.example.com --turn-user username --turn-pass password
```

## Complete Command Line Options

```
Usage: ./p2p [options]
Options:
  -s                 Run as server
  -p <port>          Specify port (default: 8080)
  -c <ip>            Connect to specified IP (client mode)
  --no-ice           Disable ICE (use direct socket connection)
  --stun <server>    Specify STUN server (default: stun.l.google.com)
  --stun-port <port> Specify STUN port (default: 19302)
  --turn <server>    Specify TURN server
  --turn-port <port> Specify TURN port (default: 3478)
  --turn-user <user> Specify TURN username
  --turn-pass <pass> Specify TURN password
  --help             Show this help message
```

## Using the Application

### Step 1: Establish Connection

When running with ICE enabled (the default):

1. Start both peers (server and client)
2. Each peer will display its local SDP information
3. Copy the entire SDP output from one peer and paste it into the other peer
4. Press Enter twice after pasting to signal the end of input
5. Repeat the process for the other direction (copy SDP from second peer to first)
6. The application will attempt to establish a connection

### Step 2: Exchange Messages

Once connected:
1. Type messages in either terminal
2. Messages will be sent to the remote peer and displayed
3. Press Ctrl+C to exit

## Troubleshooting

If you encounter issues:

1. **Compilation Problems**
   - Ensure all dependencies are installed
   - Check for compiler errors in the output

2. **Connection Issues**
   - Verify that both peers have internet connectivity
   - Try using different STUN servers
   - Consider using a TURN server if direct connection fails
   - Ensure firewalls are not blocking the connection

3. **SDP Exchange Problems**
   - Make sure to copy the complete SDP information
   - Press Enter twice after pasting to submit the input

## Setting Up Your Own TURN Server

For reliable NAT traversal, you can set up your own TURN server using coturn:

1. Install coturn:
   ```bash
   sudo apt-get install coturn
   ```

2. Configure coturn:
   ```bash
   sudo nano /etc/turnserver.conf
   ```
   
   Add these lines:
   ```
   listening-port=3478
   fingerprint
   lt-cred-mech
   user=username:password
   realm=yourdomain.com
   ```

3. Start the TURN server:
   ```bash
   sudo turnserver -o -a -v
   ```

4. Use this TURN server with your p2p application:
   ```bash
   ./p2p --turn your-server-ip --turn-user username --turn-pass password
   ```