/*
 * ==============================================================================
 * File: xd_readline.c
 * Author: Duraid Maihoub
 * Date: 17 June 2025
 * Description: Part of the xd-readline project.
 * Repository: https://github.com/xduraid/xd-readline
 * ==============================================================================
 * Copyright (c) 2025 Duraid Maihoub
 *
 * xd-readline is distributed under the MIT License. See the LICENSE file
 * for more information.
 * ==============================================================================
 */

#include "xd_readline.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

// ========================
// Macros and Constants
// ========================

#define XD_ASCII_NUL (0)  // ASCII for `NUL`

// ========================
// Typedefs
// ========================

// ========================
// Function Declarations
// ========================

static void xd_readline_init() __attribute__((constructor));
static void xd_readline_destroy() __attribute__((destructor));

static void xd_tty_raw();
static void xd_tty_restore();

// ========================
// Variables
// ========================

/**
 * @brief Store the tty attributes when `xd_readline()` was called.
 */
static struct termios xd_original_tty_attributes;

/**
 * @brief The input buffer.
 */
static char *xd_input_buffer = NULL;

/**
 * @brief The current capacity (max-length) of the input buffer.
 */
static int xd_input_capacity = LINE_MAX;

/**
 * @brief The current length of the input buffer.
 */
static int xd_input_length = 0;

// ========================
// Public Variables
// ========================

// ========================
// Function Definitions
// ========================

/**
 * @brief Constructor, runs before main to initialize the `xd-readline`
 * library.
 */
static void xd_readline_init() {
  // initialize input buffer
  xd_input_buffer = (char *)malloc(sizeof(char) * xd_input_capacity);
  if (xd_input_buffer == NULL) {
    fprintf(stderr, "xd_readline: failed to allocate memory: %s\n",
            strerror(errno));
    exit(EXIT_FAILURE);
  }
  xd_input_length = 0;
  xd_input_buffer[0] = XD_ASCII_NUL;
}  // xd_readline_init()

/**
 * @brief Destructor, runs before exit to cleanup after the `xd-readline`
 * library.
 */
static void xd_readline_destroy() {
  free(xd_input_buffer);
}  // xd_readline_destroy()

/**
 * @brief Changes the terminal input settings to raw.
 */
static void xd_tty_raw() {
  // store original tty attributes
  if (tcgetattr(STDIN_FILENO, &xd_original_tty_attributes) == -1) {
    fprintf(stderr, "xd_readline: failed to get tty attributes\n");
    exit(EXIT_FAILURE);
  }

  // set tty input to raw
  struct termios xd_getline_tty_attributes;
  if (tcgetattr(STDIN_FILENO, &xd_getline_tty_attributes) == -1) {
    fprintf(stderr, "xd_readline: failed to get tty attributes\n");
    exit(EXIT_FAILURE);
  }
  xd_getline_tty_attributes.c_lflag &= ~(ICANON | ECHO);
  xd_getline_tty_attributes.c_cc[VTIME] = 0;
  xd_getline_tty_attributes.c_cc[VMIN] = 1;
  if (tcsetattr(STDIN_FILENO, TCSANOW, &xd_getline_tty_attributes) == -1) {
    fprintf(stderr, "xd_readline: failed to set tty attributes\n");
    exit(EXIT_FAILURE);
  }
}  // xd_tty_raw()

/**
 * @brief Restore original terminal settings.
 */
static void xd_tty_restore() {
  if (tcsetattr(STDIN_FILENO, TCSANOW, &xd_original_tty_attributes) == -1) {
    fprintf(stderr, "xd_readline: failed to reset tty attributes\n");
    exit(EXIT_FAILURE);
  }
}  // xd_tty_restore()

// ========================
// Public Functions
// ========================

char *xd_readline() {
  xd_input_length = 0;
  xd_input_buffer[0] = XD_ASCII_NUL;

  xd_tty_raw();
  // READ
  xd_tty_restore();
  return NULL;
}  // xd_readline()
