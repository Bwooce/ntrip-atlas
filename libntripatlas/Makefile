# NTRIP Atlas Library Makefile
# Simple build configuration for Linux/macOS

CC = gcc
CXX = g++
AR = ar
CFLAGS = -Wall -Wextra -O2 -Iinclude
CXXFLAGS = $(CFLAGS) -std=c++11
LDFLAGS = -lcurl -lm

# Directories
SRC_DIR = src
PLATFORM_DIR = platforms
BUILD_DIR = build
LIB_DIR = lib

# Source files
CORE_SOURCES = $(wildcard $(SRC_DIR)/*.c)
CORE_OBJECTS = $(CORE_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Platform-specific sources
LINUX_SOURCES = $(wildcard $(PLATFORM_DIR)/linux/*.c)
LINUX_OBJECTS = $(LINUX_SOURCES:$(PLATFORM_DIR)/linux/%.c=$(BUILD_DIR)/linux_%.o)

# Library output
STATIC_LIB = $(LIB_DIR)/libntripatlas.a
SHARED_LIB = $(LIB_DIR)/libntripatlas.so

.PHONY: all clean linux esp32 windows

all: linux

# Linux build
linux: $(STATIC_LIB) $(SHARED_LIB)

# Create directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(LIB_DIR):
	mkdir -p $(LIB_DIR)

# Compile core sources
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

# Compile Linux platform
$(BUILD_DIR)/linux_%.o: $(PLATFORM_DIR)/linux/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

# Static library
$(STATIC_LIB): $(CORE_OBJECTS) $(LINUX_OBJECTS) | $(LIB_DIR)
	$(AR) rcs $@ $^

# Shared library
$(SHARED_LIB): $(CORE_OBJECTS) $(LINUX_OBJECTS) | $(LIB_DIR)
	$(CC) -shared -o $@ $^ $(LDFLAGS)

# ESP32 build (requires Arduino CLI or PlatformIO)
esp32:
	@echo "ESP32 build requires Arduino CLI or PlatformIO"
	@echo "See examples/esp32_streaming_example.cpp for Arduino sketch"

# Windows build (requires MinGW or MSVC)
windows:
	@echo "Windows build requires MinGW or MSVC"
	@echo "Use Visual Studio solution or CMake for Windows builds"

# Clean
clean:
	rm -rf $(BUILD_DIR) $(LIB_DIR)

# Install (Linux only)
install: linux
	@echo "Installing to /usr/local..."
	@sudo mkdir -p /usr/local/include/ntrip_atlas
	@sudo cp include/*.h /usr/local/include/ntrip_atlas/
	@sudo cp $(STATIC_LIB) /usr/local/lib/
	@sudo cp $(SHARED_LIB) /usr/local/lib/
	@sudo ldconfig

# Uninstall
uninstall:
	@echo "Uninstalling from /usr/local..."
	@sudo rm -rf /usr/local/include/ntrip_atlas
	@sudo rm -f /usr/local/lib/libntripatlas.*
	@sudo ldconfig

# Help
help:
	@echo "NTRIP Atlas Library Build"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build for current platform (default: Linux)"
	@echo "  linux    - Build Linux version (static + shared libs)"
	@echo "  esp32    - Instructions for ESP32 build"
	@echo "  windows  - Instructions for Windows build"
	@echo "  clean    - Remove build artifacts"
	@echo "  install  - Install library (Linux only, requires sudo)"
	@echo "  uninstall- Uninstall library (Linux only, requires sudo)"
	@echo ""
	@echo "Examples:"
	@echo "  make         # Build Linux libraries"
	@echo "  make clean   # Clean build"
	@echo "  make install # Install to system"
