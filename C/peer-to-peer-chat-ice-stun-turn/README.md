# P2P Communication with STUN/TURN NAT Traversal

A peer-to-peer communication application that works across different networks, enabling direct communication between peers even when they are behind NAT.

![P2P Communication](https://user-images.githubusercontent.com/12345678/example-image.png)

## Features

- **Direct Socket Communication**: Simple peer-to-peer messaging over TCP
- **NAT Traversal**: Communicate between peers behind different NATs
- **STUN Support**: Discover public IP and port for connectivity
- **TURN Support**: Relay communication when direct connection fails
- **ICE Protocol**: Automatically find the optimal connection path
- **Modular Design**: Cleanly separated components for easy understanding and extension

## Quick Start

### Prerequisites

- GLib 2.0 development libraries
- libnice (for ICE/STUN/TURN support)
- Compiler toolchain (gcc, make)

### Build

```bash
# Install dependencies (Ubuntu/Debian)
make install-deps

# Build the application
make
```

### Run

For local testing:
```bash
# Terminal 1 (Server)
./p2p -s --no-ice

# Terminal 2 (Client)
./p2p -c 127.0.0.1 --no-ice
```

For NAT traversal using STUN:
```bash
# On machine A (Server)
./p2p -s --stun stun.l.google.com

# On machine B (Client)
./p2p --stun stun.l.google.com
```

Follow the prompts to exchange SDP information between peers.

## How It Works

1. **Discovery**: Peers discover their public IP addresses using STUN servers
2. **Candidate Exchange**: Peers exchange potential connection paths via SDP
3. **Connectivity Checks**: ICE protocol tests all possible connection paths
4. **Connection**: Peers establish the best available connection
5. **Communication**: Messages flow directly between peers (or via TURN if needed)

## Documentation

- [Setup Instructions](setup.md): Detailed build and run instructions
- [Project Knowledge Base](project_knowledge.md): Technical concepts and implementation details
- [Code Documentation](include/): Header files with function documentation

## Repository Structure

```
├── include/            # Header files
│   ├── common.h        # Common structures and definitions
│   ├── ice.h           # ICE protocol and STUN/TURN functionality
│   └── socket.h        # Socket communication
├── src/                # Source files
│   ├── ice.c           # ICE implementation
│   ├── main.c          # Main application
│   └── socket.c        # Socket implementation
├── build/              # Compiled object files
├── Makefile            # Build system
├── README.md           # This file
├── setup.md            # Setup instructions
└── project_knowledge.md # Technical documentation
```

## Command Line Options

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

## Use Cases

- **Direct Messaging**: Personal communication between peers
- **File Sharing**: Transfer files directly between computers
- **Gaming**: Low-latency direct connections for multiplayer games
- **IoT**: Communication between devices in different networks
- **Building Block**: Foundation for more complex P2P applications

## Limitations

- Manual SDP exchange (could be automated with a signaling server)
- Text-based messaging only (could be extended for media)
- No encryption (could add DTLS)
- Limited to TCP (could support UDP for real-time applications)

## Contributing

Contributions are welcome! Here are some areas for improvement:

- Add encryption for secure communication
- Implement a signaling server for automatic SDP exchange
- Add UDP support for real-time applications
- Extend with audio/video capabilities
- Improve error handling and recovery

## License

This project is available under the MIT License. See the LICENSE file for details.

## Acknowledgements

- [libnice](https://nice.freedesktop.org/) for ICE/STUN/TURN implementation
- [GLib](https://developer.gnome.org/glib/) for support libraries
- Public STUN servers that make NAT traversal possible