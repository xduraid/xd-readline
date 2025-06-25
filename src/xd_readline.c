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

#include <asm-generic/ioctls.h>
#include <bits/posix2_lim.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
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

// ASCII control characters

#define XD_ASCII_NUL (0)    // ASCII for `NUL`
#define XD_ASCII_SOH (1)    // ASCII for `SOH` (`Ctrl+A`)
#define XD_ASCII_STX (2)    // ASCII for `STX` (`Ctrl+B`)
#define XD_ASCII_EOT (4)    // ASCII for `EOT` (`Ctrl+D`)
#define XD_ASCII_ENQ (5)    // ASCII for `ENQ` (`Ctrl+E`)
#define XD_ASCII_ACK (6)    // ASCII for `ACK` (`Ctrl+F`)
#define XD_ASCII_BEL (7)    // ASCII for `BEL` (`Ctrl+G`)
#define XD_ASCII_BS  (8)    // ASCII for `BS` (`Ctrl+H`)
#define XD_ASCII_LF  (10)   // ASCII for `LF` (`Enter`)
#define XD_ASCII_VT  (11)   // ASCII for `VT` (`Ctrl+K`)
#define XD_ASCII_FF  (12)   // ASCII for `FF` (`Ctrl+L`)
#define XD_ASCII_NAK (21)   // ASCII for `NAK` (`Ctrl+U`)
#define XD_ASCII_ESC (27)   // ASCII for `ESC` (`Esc`)
#define XD_ASCII_DEL (127)  // ASCII for `DEL` (`Backspace`)

// ANSI escape sequences for keyboard shortcuts

#define XD_ANSI_UP_ARROW    "\033[A"   // ANSI for `Up Arrow` key
#define XD_ANSI_DOWN_ARROW  "\033[B"   // ANSI for `Down Arrow` key
#define XD_ANSI_RIGHT_ARROW "\033[C"   // ANSI for `Right Arrow` key
#define XD_ANSI_LEFT_ARROW  "\033[D"   // ANSI for `Left Arrow` key
#define XD_ANSI_HOME        "\033[H"   // ANSI for `Home` key
#define XD_ANSI_END         "\033[F"   // ANSI for `End` key
#define XD_ANSI_DELETE      "\033[3~"  // ANSI for `Delete` key

#define XD_ANSI_ALT_F "\033f"  // ANSI for `ALT+F` key binding
#define XD_ANSI_ALT_B "\033b"  // ANSI for `ALT+B` key binding
#define XD_ANSI_ALT_D "\033d"  // ANSI for `ALT+D` key binding

#define XD_ANSI_ALT_BS "\033\177"  // ANSI for `ALT+Backspace` key binding

#define XD_ANSI_CTRL_RARROW "\033[1;5C"  // ANSI for `Ctrl+Right Arrow` binding
#define XD_ANSI_CTRL_LARROW "\033[1;5D"  // ANSI for `Ctrl+Left Arrow` binding
#define XD_ANSI_CTRL_DELETE "\033[3;5~"  // ANSI for `Ctrl+Delete` binding

// ANSI sequences' formats

#define XD_ANSI_CRSR_SET_COL "\033[%dG"   // ANSI for setting cursor column
#define XD_ANSI_CRSR_MV_HOME "\033[H"     // ANSI for moving cursor to (1, 1)
#define XD_ANSI_CRSR_MV_UP   "\033[%dA"   // ANSI for moving cursor up
#define XD_ANSI_CRSR_MV_DN   "\033[%dB"   // ANSI for moving cursor down
#define XD_ANSI_LINE_CLR     "\033[2K\r"  // ANSI for clearing current line
#define XD_ANSI_SCRN_CLR     "\033[2J"    // ANSI for clearing the screen

// ========================
// Typedefs
// ========================

/**
 * @brief Input handler function type.
 */
typedef void (*xd_input_handler_func)(void);

/**
 * @brief Represents an ANSI escape sequence to input handler function binding.
 */
typedef struct xd_esc_seq_binding_t {
  const char *sequence;                 // The escape sequence string.
  const xd_input_handler_func handler;  // The handler function.
} xd_esc_seq_binding_t;

/**
 * @brief Represents a history entry.
 */
typedef struct xd_history_entry_t {
  char *str;     // The history string.
  int capacity;  // The capacity of the history string.
  int length;    // The length of the history string.
} xd_history_entry_t;

// ========================
// Function Declarations
// ========================

static void xd_readline_init() __attribute__((constructor));
static void xd_readline_destroy() __attribute__((destructor));

static void xd_readline_history_init();
static void xd_readline_history_destroy();

static void xd_input_buffer_insert(char chr);
static void xd_input_buffer_remove_before_cursor(int n);
static void xd_input_buffer_remove_from_cursor(int n);

static int xd_input_buffer_get_current_word_end();
static int xd_input_buffer_get_current_word_start();

static void xd_input_buffer_save_to_history();
static void xd_input_buffer_load_from_history();

static void xd_tty_raw();
static void xd_tty_restore();

static inline void xd_tty_bell();

static void xd_tty_input_clear();
static void xd_tty_input_redraw();

static void xd_tty_screen_resize();

static void xd_tty_write_ansii_sequence(const char *format, ...);
static void xd_tty_write(const void *data, int length);
static void xd_tty_write_track(const void *data, int length);

static void xd_tty_cursor_move_left_wrap(int n);
static void xd_tty_cursor_move_right_wrap(int n);

static void xd_input_handle_printable(char chr);

static void xd_input_handle_ctrl_a();
static void xd_input_handle_ctrl_b();
static void xd_input_handle_ctrl_d();
static void xd_input_handle_ctrl_e();
static void xd_input_handle_ctrl_f();
static void xd_input_handle_ctrl_g();
static void xd_input_handle_ctrl_h();
static void xd_input_handle_ctrl_k();
static void xd_input_handle_ctrl_l();
static void xd_input_handle_ctrl_u();

static void xd_input_handle_backspace();
static void xd_input_handle_enter();

static void xd_input_handle_up_arrow();
static void xd_input_handle_down_arrow();
static void xd_input_handle_right_arrow();
static void xd_input_handle_left_arrow();

static void xd_input_handle_home();
static void xd_input_handle_end();
static void xd_input_handle_delete();

static void xd_input_handle_ctrl_right_arrow();
static void xd_input_handle_ctrl_left_arrow();
static void xd_input_handle_ctrl_delete();

static void xd_input_handle_alt_f();
static void xd_input_handle_alt_b();
static void xd_input_handle_alt_d();
static void xd_input_handle_alt_backspace();

static void xd_input_handle_escape_sequence();

static void xd_input_handle_control(char chr);

static void xd_input_handler(char chr);

static void xd_sigwinch_handler(int sig_num);

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
 * @brief Indicates whether `SIGWINCH` signal has been received.
 */
static volatile sig_atomic_t xd_tty_win_resized = 0;

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
 * @brief The logical position of the cursor within the input buffer.
 */
static int xd_input_cursor = 0;

/**
 * @brief Indicates whether to redraw the prompt and input befor reading another
 * character (non-zero) or not (zero).
 */
static int xd_readline_redraw = 0;

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

/**
 * @brief The history array (circular buffer)
 */
static xd_history_entry_t **xd_history = NULL;

/**
 * @brief Index of the current history entry.
 */
static int xd_history_nav_idx = XD_HISTORY_MAX;

/**
 * @brief Index of the first history entry.
 */
static int xd_history_start_idx = 0;

/**
 * @brief Index of the last history entry.
 */
static int xd_history_end_idx = XD_HISTORY_MAX - 1;

/**
 * @brief The number of entries currently stored in history.
 */
static int xd_history_length = 0;

/**
 * @brief Array mapping ANSI escape sequences to corresponding input
 * handlers.
 */
static const xd_esc_seq_binding_t xd_esc_seq_bindings[] = {
    {XD_ANSI_UP_ARROW,    xd_input_handle_up_arrow        },
    {XD_ANSI_DOWN_ARROW,  xd_input_handle_down_arrow      },
    {XD_ANSI_RIGHT_ARROW, xd_input_handle_right_arrow     },
    {XD_ANSI_LEFT_ARROW,  xd_input_handle_left_arrow      },
    {XD_ANSI_HOME,        xd_input_handle_home            },
    {XD_ANSI_END,         xd_input_handle_end             },
    {XD_ANSI_DELETE,      xd_input_handle_delete          },
    {XD_ANSI_ALT_F,       xd_input_handle_alt_f           },
    {XD_ANSI_ALT_B,       xd_input_handle_alt_b           },
    {XD_ANSI_ALT_D,       xd_input_handle_alt_d           },
    {XD_ANSI_ALT_BS,      xd_input_handle_alt_backspace   },
    {XD_ANSI_CTRL_RARROW, xd_input_handle_ctrl_right_arrow},
    {XD_ANSI_CTRL_LARROW, xd_input_handle_ctrl_left_arrow },
    {XD_ANSI_CTRL_DELETE, xd_input_handle_ctrl_delete     },
};

/**
 * @brief Number of defined escape sequence bindings.
 */
static const int xd_esc_seq_bindings_length =
    sizeof(xd_esc_seq_bindings) / sizeof(xd_esc_seq_bindings[0]);

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

  // register `SIGWINCH` singal handler
  if (signal(SIGWINCH, xd_sigwinch_handler) == SIG_ERR) {
    fprintf(stderr, "xd_readline: failed to set `SIGWINCH` handler: \n");
    exit(EXIT_FAILURE);
  }

  // initialize the history
  xd_readline_history_init();

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
  xd_readline_history_destroy();
  free(xd_input_buffer);
}  // xd_readline_destroy()

/**
 * @brief Initialize the history array by allocating all needed memory up-front
 * to reduce the allocation-deallocation overhead.
 *
 * @note This function must be called first thing inside the constructor
 * `xd_readline_init()` beacuse it calls `exit()` on failure.
 */
static void xd_readline_history_init() {
  xd_history = (xd_history_entry_t **)malloc(sizeof(xd_history_entry_t *) *
                                             (XD_HISTORY_MAX + 1));
  if (xd_history == NULL) {
    fprintf(stderr, "xd_readline: failed to allocate memory: %s\n",
            strerror(errno));
    exit(EXIT_FAILURE);
  }

  for (int i = 0; i <= XD_HISTORY_MAX; i++) {
    xd_history[i] = (xd_history_entry_t *)malloc(sizeof(xd_history_entry_t));
    if (xd_history[i] == NULL) {
      fprintf(stderr, "xd_readline: failed to allocate memory: %s\n",
              strerror(errno));
      exit(EXIT_FAILURE);
    }

    xd_history[i]->capacity = LINE_MAX;
    xd_history[i]->length = 0;
    xd_history[i]->str = (char *)malloc(sizeof(char) * LINE_MAX);
    if (xd_history[i]->str == NULL) {
      free(xd_history[i]);
      xd_history[i] = NULL;
      fprintf(stderr, "xd_readline: failed to allocate memory: %s\n",
              strerror(errno));
      exit(EXIT_FAILURE);
    }
    xd_history[i]->str[0] = XD_ASCII_NUL;
  }
}  // xd_readline_history_init()

/**
 * @brief Frees the resources used for the history.
 */
static void xd_readline_history_destroy() {
  for (int i = 0; i <= XD_HISTORY_MAX; i++) {
    if (xd_history[i] == NULL) {
      break;
    }
    free(xd_history[i]->str);
    free(xd_history[i]);
  }
  free((void *)xd_history);
}  // xd_readline_history_destroy()

/**
 * @brief Inserts the passed character into the input buffer at the cursor
 * position.
 *
 * @param chr The character to be inserted.
 */
static void xd_input_buffer_insert(char chr) {
  // shift all the characters starting from the cursor by one to the right
  for (int i = xd_input_length; i > xd_input_cursor; i--) {
    xd_input_buffer[i] = xd_input_buffer[i - 1];
  }
  // insert the new character
  xd_input_buffer[xd_input_cursor++] = chr;
  xd_input_buffer[++xd_input_length] = XD_ASCII_NUL;
}  // xd_input_buffer_insert()

/**
 * @brief Removes a number of characters before the cursor from the input
 * buffer.
 *
 * @param n The number of characters to be removed
 */
static void xd_input_buffer_remove_before_cursor(int n) {
  if (xd_input_cursor < n) {
    return;
  }

  // shift all characters starting from the cursor by n to the left
  for (int i = xd_input_cursor; i < xd_input_length; i++) {
    xd_input_buffer[i - n] = xd_input_buffer[i];
  }
  xd_input_cursor -= n;
  xd_input_length -= n;
}  // xd_input_buffer_remove_before_cursor()

/**
 * @brief Removes a number of characters starting at the cursor position from
 * the input buffer.
 *
 * @param n The number of characters to be removed
 */
static void xd_input_buffer_remove_from_cursor(int n) {
  if (xd_input_length - xd_input_cursor < n) {
    return;
  }

  // shift the characters after the ones being removed by n to the left
  for (int i = xd_input_cursor; i < xd_input_length - n; i++) {
    xd_input_buffer[i] = xd_input_buffer[i + n];
  }
  xd_input_length -= n;
}  // xd_input_buffer_remove_from_cursor()

/**
 * @brief Finds the end position of the current word in the input
 * buffer.
 *
 * @return The index of the current word end.
 */
static int xd_input_buffer_get_current_word_end() {
  int idx = xd_input_cursor;
  // skip all non-alphanumeric characters
  while (idx < xd_input_length && !isalnum(xd_input_buffer[idx])) {
    idx++;
  }
  // skip the word
  while (idx < xd_input_length && isalnum(xd_input_buffer[idx])) {
    idx++;
  }
  return idx;
}  // xd_input_buffer_get_current_word_end()

/**
 * @brief Finds the start position of the current word in the input
 * buffer.
 *
 * @return The index of the current word start.
 */
static int xd_input_buffer_get_current_word_start() {
  int idx = xd_input_cursor;
  // skip all non-alphanumeric characters
  while (idx > 0 && !isalnum(xd_input_buffer[idx - 1])) {
    idx--;
  }
  // skip the word
  while (idx > 0 && isalnum(xd_input_buffer[idx - 1])) {
    idx--;
  }
  return idx;
}  // xd_input_buffer_get_current_word_start()

/**
 * @brief Saves the contents of the input buffer to the current navigation entry
 * in the history (at `xd_history_nav_idx`).
 */
static void xd_input_buffer_save_to_history() {
  xd_history_entry_t *history_entry = xd_history[xd_history_nav_idx];

  // resize the history entry string if needed
  if (xd_input_length > history_entry->capacity - 1) {
    // resize to multiple of `LINE_MAX`
    int new_capacity = xd_input_length + 1;
    if (new_capacity % LINE_MAX != 0) {
      new_capacity += LINE_MAX - (new_capacity % LINE_MAX);
    }

    char *ptr = (char *)realloc(history_entry->str, new_capacity);
    if (ptr == NULL) {
      return;  // allocation error, stop saving
    }
    history_entry->capacity = new_capacity;
    history_entry->str = ptr;
  }

  memcpy(history_entry->str, xd_input_buffer, xd_input_length);
  history_entry->str[xd_input_length] = XD_ASCII_NUL;
  history_entry->length = xd_input_length;
}  // xd_input_buffer_save_to_history()

/**
 * @brief Loads the contents of the current navigation entry in the history (at
 * `xd_history_nav_idx`) to the input buffer.
 */
static void xd_input_buffer_load_from_history() {
  xd_history_entry_t *history_entry = xd_history[xd_history_nav_idx];

  // resize the input buffer if needed
  if (history_entry->length > xd_input_capacity - 1) {
    // resize to multiple of `LINE_MAX`
    int new_capacity = history_entry->length + 1;
    if (new_capacity % LINE_MAX != 0) {
      new_capacity += LINE_MAX - (new_capacity % LINE_MAX);
    }

    char *ptr = (char *)realloc(xd_input_buffer, new_capacity);
    if (ptr == NULL) {
      return;  // allocation error, stop loading
    }
    xd_input_capacity = new_capacity;
    xd_input_buffer = ptr;
  }

  xd_input_length = history_entry->length;
  xd_input_cursor = xd_input_length;
  memcpy(xd_input_buffer, history_entry->str, xd_input_length);
  xd_input_buffer[xd_input_length] = XD_ASCII_NUL;
}  // xd_input_buffer_load_from_history()

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
 * @brief Write bell character to terminal to make an alert sound.
 */
static inline void xd_tty_bell() {
  char chr = XD_ASCII_BEL;
  xd_tty_write(&chr, 1);
}  // xd_tty_bell_sound()

/**
 * @brief Clears the prompt and the temrinal input and puts the cursor at the
 * beginning.
 */
static void xd_tty_input_clear() {
  // move to the end of the input
  int cursor_flat_pos =
      ((xd_tty_cursor_row - 1) * xd_tty_win_width) + xd_tty_cursor_col - 1;
  xd_tty_cursor_move_right_wrap(xd_tty_chars_count - cursor_flat_pos);

  // clear all rows one by one bottom-up
  int rows = (xd_tty_chars_count + xd_tty_win_width) / xd_tty_win_width;
  for (int i = 0; i < rows; i++) {
    xd_tty_write_ansii_sequence(XD_ANSI_LINE_CLR);
    xd_tty_cursor_col = 1;
    if (i < rows - 1) {
      xd_tty_write_ansii_sequence(XD_ANSI_CRSR_MV_UP, 1);
      xd_tty_cursor_row--;
    }
  }
  xd_tty_chars_count = 0;
}  // xd_tty_input_clear()

/**
 * @brief Clears the prompt and the input then re-writes them and puts the
 * cursor in its proper position.
 */
static void xd_tty_input_redraw() {
  xd_tty_input_clear();
  xd_tty_write_track(xd_readline_prompt, xd_readline_prompt_length);
  xd_tty_write_track(xd_input_buffer, xd_input_length);
  xd_tty_cursor_move_left_wrap(xd_input_length - xd_input_cursor);
}  // xd_tty_input_redraw()

/**
 * @brief Handles terminal screen resize by getting the new window width and
 * calculating the new cursor position.
 */
static void xd_tty_screen_resize() {
  struct winsize wsz;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &wsz) == 0) {
    int cursor_flat_pos =
        ((xd_tty_cursor_row - 1) * xd_tty_win_width) + xd_tty_cursor_col - 1;
    xd_tty_win_width = wsz.ws_col;
    xd_tty_cursor_row = (cursor_flat_pos / xd_tty_win_width) + 1;
    xd_tty_cursor_col = (cursor_flat_pos % xd_tty_win_width) + 1;
    xd_readline_redraw = 1;
  }
}  // xd_tty_screen_resize()

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
 * @brief Moves the terminal cursor left by a specified number of columns,
 * wrapping across rows as needed.
 *
 * @param n The number of columns to move the cursor to the left.
 */
static inline void xd_tty_cursor_move_left_wrap(int n) {
  if (n == 0) {
    return;
  }
  int cursor_flat_pos =
      ((xd_tty_cursor_row - 1) * xd_tty_win_width) + xd_tty_cursor_col - n - 1;
  int new_cursor_row = (cursor_flat_pos / xd_tty_win_width) + 1;
  int new_cursor_col = (cursor_flat_pos % xd_tty_win_width) + 1;
  if (new_cursor_row != xd_tty_cursor_row) {
    xd_tty_write_ansii_sequence(XD_ANSI_CRSR_MV_UP,
                                xd_tty_cursor_row - new_cursor_row);
    xd_tty_cursor_row = new_cursor_row;
  }
  xd_tty_write_ansii_sequence(XD_ANSI_CRSR_SET_COL, new_cursor_col);
  xd_tty_cursor_col = new_cursor_col;
}  // xd_tty_cursor_move_left_wrap()

/**
 * @brief Moves the terminal cursor right by a specified number of columns,
 * wrapping across rows as needed.
 *
 * @param n The number of columns to move the cursor to the right.
 */
static inline void xd_tty_cursor_move_right_wrap(int n) {
  if (n == 0) {
    return;
  }
  int cursor_flat_pos =
      ((xd_tty_cursor_row - 1) * xd_tty_win_width) + xd_tty_cursor_col + n - 1;
  int new_cursor_row = (cursor_flat_pos / xd_tty_win_width) + 1;
  int new_cursor_col = (cursor_flat_pos % xd_tty_win_width) + 1;
  if (new_cursor_row != xd_tty_cursor_row) {
    xd_tty_write_ansii_sequence(XD_ANSI_CRSR_MV_DN,
                                new_cursor_row - xd_tty_cursor_row);
    xd_tty_cursor_row = new_cursor_row;
  }
  xd_tty_write_ansii_sequence(XD_ANSI_CRSR_SET_COL, new_cursor_col);
  xd_tty_cursor_col = new_cursor_col;
}  // xd_tty_cursor_move_right_wrap()

/**
 * @brief Handles the case where the input is a printable character.
 *
 * @param chr the input character.
 */
static void xd_input_handle_printable(char chr) {
  xd_input_buffer_insert(chr);
  if (xd_input_cursor == xd_input_length) {
    xd_tty_write_track(&chr, 1);
    // don't redraw when adding to the end
    return;
  }
  xd_readline_redraw = 1;
}  // xd_input_handle_printable()

/**
 * @brief Handles the case where the input is `Ctrl+A`.
 */
static void xd_input_handle_ctrl_a() {
  if (xd_input_cursor == 0) {
    return;
  }
  xd_tty_cursor_move_left_wrap(xd_input_cursor);
  xd_input_cursor = 0;
}  // xd_input_handle_ctrl_a()

/**
 * @brief Handles the case where the input is `Ctrl+B`.
 */
static void xd_input_handle_ctrl_b() {
  if (xd_input_cursor == 0) {
    xd_tty_bell();
    return;
  }
  xd_tty_cursor_move_left_wrap(1);
  xd_input_cursor--;
}  // xd_input_handle_ctrl_b()

/**
 * @brief Handles the case where the input is `Ctrl+D`.
 */
static void xd_input_handle_ctrl_d() {
  if (xd_input_length == 0) {
    xd_readline_finished = 1;
    xd_readline_return = NULL;
    return;
  }
  xd_input_handle_delete();
}  // xd_input_handle_ctrl_d()

/**
 * @brief Handles the case where the input is `Ctrl+E`.
 */
static void xd_input_handle_ctrl_e() {
  if (xd_input_cursor == xd_input_length) {
    return;
  }
  xd_tty_cursor_move_right_wrap(xd_input_length - xd_input_cursor);
  xd_input_cursor = xd_input_length;
}  // xd_input_handle_ctrl_e()

/**
 * @brief Handles the case where the input is `Ctrl+F`.
 */
static void xd_input_handle_ctrl_f() {
  if (xd_input_cursor == xd_input_length) {
    xd_tty_bell();
    return;
  }
  xd_tty_cursor_move_right_wrap(1);
  xd_input_cursor++;
}  // xd_input_handle_ctrl_f()

/**
 * @brief Handles the case where the input is `Ctrl+G`.
 */
static void xd_input_handle_ctrl_g() {
  xd_tty_bell();
}  // xd_input_handle_ctrl_g()

/**
 * @brief Handles the case where the input is `Ctrl+H`.
 */
static void xd_input_handle_ctrl_h() {
  if (xd_input_cursor == 0) {
    xd_tty_bell();
    return;
  }
  xd_input_buffer_remove_before_cursor(1);
  xd_readline_redraw = 1;
}  // xd_input_handle_ctrl_h()

/**
 * @brief Handles the case where the input is `Ctrl+K`.
 */
static void xd_input_handle_ctrl_k() {
  if (xd_input_cursor == xd_input_length) {
    xd_tty_bell();
    return;
  }
  xd_input_buffer_remove_from_cursor(xd_input_length - xd_input_cursor);
  xd_readline_redraw = 1;
}  // xd_input_handle_ctrl_k()

/**
 * @brief Handles the case where the input is `Ctrl+L`.
 */
static void xd_input_handle_ctrl_l() {
  xd_tty_write_ansii_sequence(XD_ANSI_SCRN_CLR);
  xd_tty_write_ansii_sequence(XD_ANSI_CRSR_MV_HOME);
  xd_tty_cursor_row = 1;
  xd_tty_cursor_col = 1;
  xd_readline_redraw = 1;
}  // xd_input_handle_ctrl_l()

/**
 * @brief Handles the case where the input is `Ctrl+L`.
 */
static void xd_input_handle_ctrl_u() {
  if (xd_input_cursor == 0) {
    xd_tty_bell();
    return;
  }
  xd_input_buffer_remove_before_cursor(xd_input_cursor);
  xd_readline_redraw = 1;
}  // xd_input_handle_ctrl_u()

/**
 * @brief Handles the case where the input is the`Backspace` key.
 */
static void xd_input_handle_backspace() {
  xd_input_handle_ctrl_h();
}  // xd_input_handle_backspace()

/**
 * @brief Handles the case where the input is the `Enter` key.
 */
static void xd_input_handle_enter() {
  xd_input_buffer[xd_input_length++] = XD_ASCII_LF;
  xd_input_buffer[xd_input_length] = XD_ASCII_NUL;
  xd_readline_finished = 1;
  xd_tty_cursor_move_right_wrap(xd_input_length - xd_input_cursor - 1);
}  // xd_input_handle_enter()

/**
 * @brief Handles the case where the input is the `Up Arrow` key.
 */
static void xd_input_handle_up_arrow() {
  if (xd_history_length == 0 || xd_history_nav_idx == xd_history_start_idx) {
    xd_tty_bell();
    return;
  }

  xd_input_buffer_save_to_history();
  if (xd_history_nav_idx == XD_HISTORY_MAX) {
    xd_history_nav_idx = xd_history_end_idx;
  }
  else {
    xd_history_nav_idx =
        (xd_history_nav_idx - 1 + XD_HISTORY_MAX) % XD_HISTORY_MAX;
  }
  xd_input_buffer_load_from_history();
  xd_readline_redraw = 1;
}  // xd_input_handle_up_arrow()

/**
 * @brief Handles the case where the input is the `Down Arrow` key.
 */
static void xd_input_handle_down_arrow() {
  if (xd_history_length == 0 || xd_history_nav_idx == XD_HISTORY_MAX) {
    xd_tty_bell();
    return;
  }

  xd_input_buffer_save_to_history();
  if (xd_history_nav_idx == xd_history_end_idx) {
    xd_history_nav_idx = XD_HISTORY_MAX;
  }
  else {
    xd_history_nav_idx = (xd_history_nav_idx + 1) % XD_HISTORY_MAX;
  }
  xd_input_buffer_load_from_history();
  xd_readline_redraw = 1;
}  // xd_input_handle_down_arrow()

/**
 * @brief Handles the case where the input is the `Right Arrow` key.
 */
static void xd_input_handle_right_arrow() {
  xd_input_handle_ctrl_f();
}  // xd_input_handle_right_arrow()

/**
 * @brief Handles the case where the input is the `Left Arrow` key.
 */
static void xd_input_handle_left_arrow() {
  xd_input_handle_ctrl_b();
}  // xd_input_handle_left_arrow()

/**
 * @brief Handles the case where the input is the `Home` key.
 */
static void xd_input_handle_home() {
  xd_input_handle_ctrl_a();
}  // xd_input_handle_home()

/**
 * @brief Handles the case where the input is the `End` key.
 */
static void xd_input_handle_end() {
  xd_input_handle_ctrl_e();
}  // xd_input_handle_end()

/**
 * @brief Handles the case where the input is the `Delete` key.
 */
static void xd_input_handle_delete() {
  if (xd_input_cursor == xd_input_length) {
    xd_tty_bell();
    return;
  }
  xd_input_buffer_remove_from_cursor(1);
  xd_readline_redraw = 1;
}  // xd_input_handle_delete()

/**
 * @brief Handles the case where the input is `Ctrl+Right Arrow`.
 */
static void xd_input_handle_ctrl_right_arrow() {
  xd_input_handle_alt_f();
}  // xd_input_handle_ctrl_right_arrow()

/**
 * @brief Handles the case where the input is `Ctrl+Left Arrow`.
 */
static void xd_input_handle_ctrl_left_arrow() {
  xd_input_handle_alt_b();
}  // xd_input_handle_ctrl_left_arrow()

/**
 * @brief Handles the case where the input is `Ctrl+Delete`.
 */
static void xd_input_handle_ctrl_delete() {
  xd_input_handle_alt_d();
}  // xd_input_handle_ctrl_delete()

/**
 * @brief Handles the case where the input is `Alt+F`.
 */
static void xd_input_handle_alt_f() {
  if (xd_input_cursor == xd_input_length) {
    xd_tty_bell();
    return;
  }
  int idx = xd_input_buffer_get_current_word_end();
  xd_tty_cursor_move_right_wrap(idx - xd_input_cursor);
  xd_input_cursor = idx;
}  // xd_input_handle_alt_f()

/**
 * @brief Handles the case where the input is `Alt+B`.
 */
static void xd_input_handle_alt_b() {
  if (xd_input_cursor == 0) {
    xd_tty_bell();
    return;
  }
  int idx = xd_input_buffer_get_current_word_start();
  xd_tty_cursor_move_left_wrap(xd_input_cursor - idx);
  xd_input_cursor = idx;
}  // xd_input_handle_alt_b()

/**
 * @brief Handles the case where the input is `Alt+D`.
 */
static void xd_input_handle_alt_d() {
  if (xd_input_cursor == xd_input_length) {
    xd_tty_bell();
    return;
  }
  int idx = xd_input_buffer_get_current_word_end();
  xd_input_buffer_remove_from_cursor(idx - xd_input_cursor);
  xd_readline_redraw = 1;
}  // xd_input_handle_alt_d()

/**
 * @brief Handles the case where the input is `Alt+Backspace`.
 */
static void xd_input_handle_alt_backspace() {
  if (xd_input_cursor == 0) {
    xd_tty_bell();
    return;
  }
  int idx = xd_input_buffer_get_current_word_start();
  xd_input_buffer_remove_before_cursor(xd_input_cursor - idx);
  xd_readline_redraw = 1;
}  // xd_input_handle_alt_backspace()

/**
 * @brief Handles the case where the input is an escape sequence.
 */
static void xd_input_handle_escape_sequence() {
  // buffer for reading the escape sequence
  int idx = 0;
  char buffer[XD_SMALL_BUFFER_SIZE];
  buffer[idx++] = XD_ASCII_ESC;
  buffer[idx] = XD_ASCII_NUL;

  // read the escape sequence chracters one by one
  char chr;
  int is_valid_prefix = 0;
  while (idx < XD_SMALL_BUFFER_SIZE - 1) {
    if (read(STDIN_FILENO, &chr, 1) != 1) {
      return;
    }
    buffer[idx++] = chr;
    buffer[idx] = XD_ASCII_NUL;

    is_valid_prefix = 0;

    for (int i = 0; i < xd_esc_seq_bindings_length; i++) {
      // the read sequence is a defined escape sequence binding
      if (strcmp(buffer, xd_esc_seq_bindings[i].sequence) == 0) {
        xd_esc_seq_bindings[i].handler();
        return;
      }
      // the read sequence is a prefix for a defined escape sequence binding
      if (!is_valid_prefix &&
          strncmp(buffer, xd_esc_seq_bindings[i].sequence, idx) == 0) {
        is_valid_prefix = 1;
      }
    }

    if (!is_valid_prefix) {
      break;
    }
  }
}  // xd_input_handle_escape_sequence()

/**
 * @brief Handles the case where the input is a control character.
 *
 * @param chr The input character.
 */
static void xd_input_handle_control(char chr) {
  switch (chr) {
    case XD_ASCII_SOH:
      xd_input_handle_ctrl_a();
      break;
    case XD_ASCII_STX:
      xd_input_handle_ctrl_b();
      break;
    case XD_ASCII_EOT:
      xd_input_handle_ctrl_d();
      break;
    case XD_ASCII_ENQ:
      xd_input_handle_ctrl_e();
      break;
    case XD_ASCII_ACK:
      xd_input_handle_ctrl_f();
      break;
    case XD_ASCII_BEL:
      xd_input_handle_ctrl_g();
      break;
    case XD_ASCII_BS:
      xd_input_handle_ctrl_h();
      break;
    case XD_ASCII_LF:
      xd_input_handle_enter();
      break;
    case XD_ASCII_VT:
      xd_input_handle_ctrl_k();
      break;
    case XD_ASCII_FF:
      xd_input_handle_ctrl_l();
      break;
    case XD_ASCII_NAK:
      xd_input_handle_ctrl_u();
      break;
    case XD_ASCII_ESC:
      xd_input_handle_escape_sequence();
      break;
    case XD_ASCII_DEL:
      xd_input_handle_backspace();
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
}  // xd_input_handler()

/**
 * @brief handler for `SIGWINCH` signal.
 *
 * @param sig_num The signal number.
 */
static void xd_sigwinch_handler(int sig_num) {
  (void)sig_num;  // suprees unused param
  xd_tty_win_resized = 1;
}  // xd_sigwinch_handler(int sig_num)

// ========================
// Public Functions
// ========================

char *xd_readline() {
  if (xd_readline_prompt != NULL) {
    xd_readline_prompt_length = (int)strlen(xd_readline_prompt);
  }

  xd_input_cursor = 0;
  xd_input_length = 0;
  xd_input_buffer[0] = XD_ASCII_NUL;

  xd_readline_redraw = 1;
  xd_readline_return = xd_input_buffer;
  xd_readline_finished = 0;

  xd_tty_cursor_row = 1;
  xd_tty_cursor_col = 1;
  xd_tty_chars_count = 0;

  xd_history_nav_idx = XD_HISTORY_MAX;

  xd_tty_raw();

  char chr;
  while (!xd_readline_finished) {
    if (xd_tty_win_resized) {
      xd_tty_screen_resize();
      xd_tty_win_resized = 0;
    }

    if (xd_readline_redraw) {
      xd_tty_input_redraw();
      xd_readline_redraw = 0;
    }

    // expand the input buffer
    if (xd_input_length == xd_input_capacity - 1) {
      char *ptr =
          realloc(xd_input_buffer, sizeof(char) * xd_input_capacity * 2);
      if (ptr == NULL) {
        break;
      }
      xd_input_capacity *= 2;
      xd_input_buffer = ptr;
      xd_readline_return = xd_input_buffer;
    }

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
      continue;
    }

    xd_input_handler(chr);
  }

  if (xd_tty_cursor_col != 1) {
    chr = XD_ASCII_LF;
    xd_tty_write(&chr, 1);
  }

  xd_tty_restore();
  return xd_readline_return;
}  // xd_readline()

void xd_readline_history_clear() {
  for (int i = 0; i <= XD_HISTORY_MAX; i++) {
    xd_history[i]->length = 0;
    xd_history[i]->str[0] = XD_ASCII_NUL;
  }
  xd_history_nav_idx = XD_HISTORY_MAX;
  xd_history_start_idx = 0;
  xd_history_end_idx = XD_HISTORY_MAX - 1;
  xd_history_length = 0;
}  // xd_readline_history_clear()

int xd_readline_history_add(const char *str) {
  if (str == NULL) {
    return -1;
  }

  // ignore the trailing newline at the end
  int str_length = (int)strlen(str);
  if (str_length > 0 && str[str_length - 1] == XD_ASCII_LF) {
    str_length--;
  }

  int new_end_idx = (xd_history_end_idx + 1) % XD_HISTORY_MAX;
  xd_history_entry_t *history_entry = xd_history[new_end_idx];

  // resize the history entry string if needed
  if (str_length > history_entry->capacity - 1) {
    // resize to multiple of `LINE_MAX`
    int new_capacity = str_length + 1;
    if (new_capacity % LINE_MAX != 0) {
      new_capacity += LINE_MAX - (new_capacity % LINE_MAX);
    }

    char *ptr = (char *)realloc(history_entry->str, new_capacity);
    if (ptr == NULL) {
      fprintf(stderr, "xd_readline: failed to allocate memory: %s\n",
              strerror(errno));
      return -1;
    }
    history_entry->capacity = new_capacity;
    history_entry->str = ptr;
  }

  // add to history
  if (xd_history_length < XD_HISTORY_MAX) {
    xd_history_length++;
  }
  else {
    // circular buffer is full, overwrite the oldest entry
    xd_history_start_idx = (xd_history_start_idx + 1) % XD_HISTORY_MAX;
  }
  xd_history_end_idx = new_end_idx;
  memcpy(history_entry->str, str, str_length);
  history_entry->str[str_length] = XD_ASCII_NUL;
  history_entry->length = str_length;

  return 0;
}  // xd_readline_history_add()

void xd_readline_history_print() {
  int idx = xd_history_start_idx;
  for (int i = 0; i < xd_history_length; i++) {
    printf("    %d  %s\n", i + 1, xd_history[idx]->str);
    idx = (idx + 1) % XD_HISTORY_MAX;
  }
}  // xd_readline_history_print()
