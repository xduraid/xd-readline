#
#  ==============================================================================
#  File: Makefile
#  Author: Duraid Maihoub
#  Date: 17 June 2025
#  Description: Part of the xd-readline project.
#  Repository: https://github.com/xduraid/xd-readline
#  ==============================================================================
#  Copyright (c) 2025 Duraid Maihoub
#
#  xd-readline is distributed under the MIT License. See the LICENSE file
#  for more information.
#  ==============================================================================
#

SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
BIN_DIR = bin

CC = gcc
CC_FLAGS = -std=gnu11 \
					 -Wall -Wextra -Werror \
					 -I$(INCLUDE_DIR)

CC_RELEASE_FLAGS = -O2
CC_DEBUG_FLAGS = -g -O0 -DDEBUG

VALGRIND = valgrind
VALGRIND_FLAGS = --leak-check=full \
								 --show-leak-kinds=all \
								 --errors-for-leak-kinds=all \
								 --track-origins=yes \
								 --track-fds=yes \
								 --error-exitcode=1 \
								 -s

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

TARGET = $(BIN_DIR)/xd_readline

.SUFFIXES:
.SECONDARY:
.PHONY: all release debug valgrind clean deep_clean help

all: debug

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CC_FLAGS) -o $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CC_FLAGS) -c -o $@ $<

release: CC_FLAGS += $(CC_RELEASE_FLAGS)
release: deep_clean $(TARGET)

debug: CC_FLAGS += $(CC_DEBUG_FLAGS)
debug: deep_clean $(TARGET)

valgrind: deep_clean debug
	$(VALGRIND) $(VALGRIND_FLAGS) ./$(TARGET)

clean:
	rm -rf $(BUILD_DIR)

deep_clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

help:
	@echo "Available targets:"
	@echo "  all         - Build the project (default: debug)"
	@echo "  release     - Build with release flags"
	@echo "  debug       - Build with debug flags"
	@echo "  valgrind    - Build in debug and run with valgrind"
	@echo "  clean       - Remove intermediate build artifacts"
	@echo "  deep_clean  - Remove all generated files"
	@echo "  help        - Show this message"
