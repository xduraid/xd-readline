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

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

// ========================
// Macros and Constants
// ========================

/**
 * @brief Size of a small buffer to be used for formatting ANSI sequences.
 */
#define XD_SMALL_BUFFER_SIZE (32)

// ASCII chars

#define XD_ASCII_NUL (0)   // ASCII for `NUL`
#define XD_ASCII_LF  (10)  // ASCII for `LF` (`Enter`)

// ANSI sequences' formats

#define XD_ANSI_CRSR_SET_COL "\033[%dG"  // ANSI for setting cursor column

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

static void xd_tty_write_ansii_sequence(const char *format, ...);
static void xd_tty_write(const void *data, int length);
static void xd_tty_write_track(const void *data, int length);

static void xd_input_handle_printable(char chr);

static void xd_input_handle_enter(char chr);
static void xd_input_handle_control(char chr);

static void xd_input_handler(char chr);

// ========================
// Variables
// ========================

/**
 * @brief The original tty attributes before `xd_readline()` was called.
 */
static struct termios xd_original_tty_attributes;

/**
 * @brief The terminal current window width.
 */
static int xd_tty_win_width = 0;

/**
 * @brief The terminal cursor row position (1-based) relative to the beginning
 * of the prompt.
 */
static int xd_tty_cursor_row = 1;

/**
 * @brief The terminal cursor column position (1-based) relative to the
 * beginning of the prompt.
 */
static int xd_tty_cursor_col = 1;

/**
 * @brief The number of characters currently displayed in the terminal (prompt +
 * input).
 */
static int xd_tty_chars_count = 0;

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

/**
 * @brief Indicates whether readline finished (non-zero) or not (zero).
 */
static int xd_readline_finished = 0;

/**
 * @brief The pointer to be returned when `xd_readline()` finishes.
 */
static char *xd_readline_return = NULL;

/**
 * @brief The length of the input prompt string.
 */
static int xd_readline_prompt_length = 0;

// ========================
// Public Variables
// ========================

const char *xd_readline_prompt = NULL;

// ========================
// Function Definitions
// ========================

/**
 * @brief Constructor, runs before main to initialize the `xd-readline`
 * library.
 */
static void xd_readline_init() {
  if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
    fprintf(stderr, "xd_readline only works with a tty IO\n");
    fprintf(stderr, "exiting...\n");
    exit(EXIT_FAILURE);
  }

  // initialize input buffer
  xd_input_buffer = (char *)malloc(sizeof(char) * xd_input_capacity);
  if (xd_input_buffer == NULL) {
    fprintf(stderr, "xd_readline: failed to allocate memory: %s\n",
            strerror(errno));
    exit(EXIT_FAILURE);
  }
  xd_input_length = 0;
  xd_input_buffer[0] = XD_ASCII_NUL;

  // get terminal window width
  struct winsize wsz;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &wsz) == -1) {
    fprintf(stderr, "xd_readline: failed to get tty window width\n");
    exit(EXIT_FAILURE);
  }
  xd_tty_win_width = wsz.ws_col;
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

/**
 * @brief Writes a formatted ANSI escape sequence to the terminal.
 *
 * @param format The format string of the ANSI sequence.
 * @param ... Variable arguments to substitute into the format string.
 */
static void xd_tty_write_ansii_sequence(const char *format, ...) {
  char buffer[XD_SMALL_BUFFER_SIZE] = {0};
  va_list args;
  va_start(args, format);
  int length = vsnprintf(buffer, XD_SMALL_BUFFER_SIZE, format, args);
  va_end(args);
  if (write(STDOUT_FILENO, buffer, length) == -1) {
    xd_tty_restore();
    fprintf(stderr, "xd_readline: failed to write to tty: %s\n",
            strerror(errno));
    exit(EXIT_FAILURE);
  }
}  // xd_tty_write_ansii_sequence()

/**
 * @brief Wrapper for `write()` used to write data to `stdout`.
 *
 * @param data Pointer to the data to be written.
 * @param length The number of bytes to be written.
 */
static void xd_tty_write(const void *data, int length) {
  if (length <= 0) {
    return;
  }
  if (write(STDOUT_FILENO, data, length) == -1) {
    xd_tty_restore();
    fprintf(stderr, "xd_readline: failed to write to tty: %s\n",
            strerror(errno));
    exit(EXIT_FAILURE);
  }
}  // xd_tty_write()

/**
 * @brief Wrapper for `write()` used to write data to `stdout` while keeping
 * track of the number of chars written and the row and column positions of the
 * cursor and updating the cursor position manually.
 *
 * @param data Pointer to the data to be written.
 * @param length The number of bytes to be written.
 */
static void xd_tty_write_track(const void *data, int length) {
  if (length <= 0) {
    return;
  }

  xd_tty_write(data, length);
  xd_tty_chars_count += length;

  // update cursor position
  int cursor_flat_pos = ((xd_tty_cursor_row - 1) * xd_tty_win_width) +
                        xd_tty_cursor_col + length - 1;
  xd_tty_cursor_row = (cursor_flat_pos / xd_tty_win_width) + 1;
  xd_tty_cursor_col = (cursor_flat_pos % xd_tty_win_width) + 1;
  if (xd_tty_cursor_col == 1) {
    // make the terminal wrap to new line
    char chr = ' ';
    xd_tty_write(&chr, 1);
  }
  xd_tty_write_ansii_sequence(XD_ANSI_CRSR_SET_COL, xd_tty_cursor_col);
}  // xd_tty_write_track()

/**
 * @brief Handles the case where the input is a printable character.
 *
 * @param chr the input character.
 */
static void xd_input_handle_printable(char chr) {
  xd_input_buffer[xd_input_length++] = chr;
  xd_input_buffer[xd_input_length] = XD_ASCII_NUL;
  xd_tty_write_track(&chr, 1);
}  // xd_input_handle_printable

/**
 * @brief Handles the case where the input is the `Enter` key.
 *
 * @param chr the input character.
 */
static void xd_input_handle_enter(char chr) {
  xd_input_buffer[xd_input_length++] = XD_ASCII_LF;
  xd_input_buffer[xd_input_length] = XD_ASCII_NUL;
  xd_readline_finished = 1;
  xd_tty_write(&chr, 1);
}  // xd_input_handle_enter()

/**
 * @brief Handles the case where the input is a control character.
 *
 * @param chr The input character.
 */
static void xd_input_handle_control(char chr) {
  switch (chr) {
    case XD_ASCII_LF:
      xd_input_handle_enter(chr);
      break;
    default:
      break;
  }
}  // xd_input_handle_control()

/**
 * @brief Handles a signle input character.
 *
 * @param chr The input character.
 */
static void xd_input_handler(char chr) {
  if (isprint(chr)) {
    xd_input_handle_printable(chr);
  }
  else if (iscntrl(chr)) {
    xd_input_handle_control(chr);
  }
}  // xd_input_handler

// ========================
// Public Functions
// ========================

char *xd_readline() {
  xd_input_length = 0;
  xd_input_buffer[0] = XD_ASCII_NUL;
  xd_readline_return = xd_input_buffer;
  xd_readline_finished = 0;

  xd_tty_cursor_row = 1;
  xd_tty_cursor_col = 1;
  xd_tty_chars_count = 0;

  xd_tty_raw();

  if (xd_readline_prompt != NULL) {
    xd_readline_prompt_length = (int)strlen(xd_readline_prompt);
    xd_tty_write_track(xd_readline_prompt, xd_readline_prompt_length);
  }

  char chr;
  while (!xd_readline_finished) {
    // read one character
    ssize_t ret = read(STDIN_FILENO, &chr, 1);
    if (ret == -1) {
      xd_tty_restore();
      fprintf(stderr, "xd_readline: failed to read from tty: %s\n",
              strerror(errno));
      exit(EXIT_FAILURE);
    }

    // EOF - input stream closed
    if (ret == 0) {
      xd_readline_finished = 1;
      xd_readline_return = NULL;
      chr = XD_ASCII_LF;
      xd_tty_write(&chr, 1);
      continue;
    }

    xd_input_handler(chr);
  }

  xd_tty_restore();
  return xd_readline_return;
}  // xd_readline()
