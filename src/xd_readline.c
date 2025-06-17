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

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

// ========================
// Macros and Constants
// ========================

// ========================
// Typedefs
// ========================

// ========================
// Function Declarations
// ========================

static void xd_tty_raw();
static void xd_tty_restore();

// ========================
// Variables
// ========================

/**
 * @brief Store the tty attributes when `xd_readline()` was called.
 */
static struct termios xd_original_tty_attributes;

// ========================
// Public Variables
// ========================

// ========================
// Function Definitions
// ========================

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
  xd_tty_raw();
  // READ
  xd_tty_restore();
  return NULL;
}  // xd_readline()
