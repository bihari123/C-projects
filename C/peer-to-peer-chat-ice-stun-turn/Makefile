CC = gcc
CFLAGS = $(shell pkg-config --cflags nice glib-2.0) -I./include -Wall -Werror -g
LDFLAGS = $(shell pkg-config --libs nice glib-2.0) -pthread

# Source directories
SRC_DIR = src
BUILD_DIR = build

# Find all source files
SRC_FILES = $(wildcard $(SRC_DIR)/*.c)
OBJ_FILES = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC_FILES))

# Main target
TARGET = p2p

all: $(BUILD_DIR) $(TARGET)

# Create build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Link the executable
$(TARGET): $(OBJ_FILES)
	$(CC) -o $@ $^ $(LDFLAGS)

# Compile sources to objects
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f $(TARGET) $(OBJ_FILES)
	rm -rf $(BUILD_DIR)

# Install dependencies (Ubuntu/Debian)
install-deps:
	sudo apt-get update
	sudo apt-get install -y libnice-dev libglib2.0-dev

# Run with default settings (server mode)
run-server:
	./$(TARGET) -s

# Run with default settings (client mode)
run-client:
	./$(TARGET) -c 127.0.0.1

# Run with STUN server (server mode)
run-stun-server:
	./$(TARGET) -s --stun stun.l.google.com

# Run with STUN server (client mode)
run-stun-client:
	./$(TARGET) --stun stun.l.google.com

.PHONY: all clean install-deps run-server run-client run-stun-server run-stun-client