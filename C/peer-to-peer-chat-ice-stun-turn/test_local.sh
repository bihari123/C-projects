#!/bin/bash

# Test script for local testing of p2p application

# Ensure we have the latest build
make clean
make

# Test direct socket connection (no ICE)
echo "Testing direct socket connection (no ICE)..."
echo "Starting server in one terminal and client in another..."

# Use gnome-terminal, xterm, or konsole based on availability
if command -v gnome-terminal &> /dev/null; then
    gnome-terminal -- bash -c "./p2p -s --no-ice; read -p 'Press Enter to close...'"
    sleep 2
    gnome-terminal -- bash -c "./p2p -c 127.0.0.1 --no-ice; read -p 'Press Enter to close...'"
elif command -v xterm &> /dev/null; then
    xterm -e "./p2p -s --no-ice; read -p 'Press Enter to close...'" &
    sleep 2
    xterm -e "./p2p -c 127.0.0.1 --no-ice; read -p 'Press Enter to close...'" &
elif command -v konsole &> /dev/null; then
    konsole -e "./p2p -s --no-ice; read -p 'Press Enter to close...'" &
    sleep 2
    konsole -e "./p2p -c 127.0.0.1 --no-ice; read -p 'Press Enter to close...'" &
else
    echo "Could not find a suitable terminal emulator."
    echo "Please run the following commands in separate terminals:"
    echo "Terminal 1: ./p2p -s --no-ice"
    echo "Terminal 2: ./p2p -c 127.0.0.1 --no-ice"
fi

echo ""
echo "For STUN/TURN testing, please use the following commands on different machines:"
echo "Machine A: ./p2p -s --stun stun.l.google.com"
echo "Machine B: ./p2p --stun stun.l.google.com"