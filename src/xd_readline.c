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
#define XD_RL_SMALL_BUFFER_SIZE (32)

/**
 * @brief The prompt for reverse history serach.
 */
#define XD_RL_REVERSE_SERACH_PROMPT "(reverse-i-search)"

/**
 * @brief The prompt for failed reverse history serach.
 */
#define XD_RL_REVERSE_SEARCH_PROMPT_FAILED "failed (reverse-i-search)"

/**
 * @brief The prompt for forward history serach.
 */
#define XD_RL_FORWARD_SERACH_PROMPT "(i-search)"

/**
 * @brief The prompt for failed forward history serach.
 */
#define XD_RL_FORWARD_SERACH_PROMPT_FAILED "failed (i-search)"

/**
 * @brief Maximum length of history search query, including null-terminator.
 */
#define XD_RL_SEARCH_QUERY_MAX LINE_MAX

/**
 * @brief Special value indicating that history search has started.
 */
#define XD_RL_SEARCH_IDX_NEW (-1)

/**
 * @brief Special value indicating that history search index is out of bounds
 * (i.e. `Ctrl+R` is pressed for the second time when navigation index is at
 * first history entry or `Ctrl+S` is pressed for the second time when the
 * navigation index is at the last history entry).
 */
#define XD_RL_SEARCH_IDX_OUT_OF_BOUNDS (-2)

// ASCII control characters

#define XD_RL_ASCII_NUL (0)    // ASCII for `NUL`
#define XD_RL_ASCII_SOH (1)    // ASCII for `SOH` (`Ctrl+A`)
#define XD_RL_ASCII_STX (2)    // ASCII for `STX` (`Ctrl+B`)
#define XD_RL_ASCII_EOT (4)    // ASCII for `EOT` (`Ctrl+D`)
#define XD_RL_ASCII_ENQ (5)    // ASCII for `ENQ` (`Ctrl+E`)
#define XD_RL_ASCII_ACK (6)    // ASCII for `ACK` (`Ctrl+F`)
#define XD_RL_ASCII_BEL (7)    // ASCII for `BEL` (`Ctrl+G`)
#define XD_RL_ASCII_BS  (8)    // ASCII for `BS` (`Ctrl+H`)
#define XD_RL_ASCII_HT  (9)    // ASCII for `HT` (`Tab`)
#define XD_RL_ASCII_LF  (10)   // ASCII for `LF` (`Enter`)
#define XD_RL_ASCII_VT  (11)   // ASCII for `VT` (`Ctrl+K`)
#define XD_RL_ASCII_FF  (12)   // ASCII for `FF` (`Ctrl+L`)
#define XD_RL_ASCII_DC2 (18)   // ASCII for `DC2` (`Ctrl+R`)
#define XD_RL_ASCII_DC3 (19)   // ASCII for `DC3` (`Ctrl+S`)
#define XD_RL_ASCII_NAK (21)   // ASCII for `NAK` (`Ctrl+U`)
#define XD_RL_ASCII_ESC (27)   // ASCII for `ESC` (`Esc`)
#define XD_RL_ASCII_DEL (127)  // ASCII for `DEL` (`Backspace`)

// ANSI escape sequences for keyboard shortcuts

#define XD_RL_ANSI_UP_ARROW    "\033[A"  // ANSI for `Up Arrow` key
#define XD_RL_ANSI_DOWN_ARROW  "\033[B"  // ANSI for `Down Arrow` key
#define XD_RL_ANSI_RIGHT_ARROW "\033[C"  // ANSI for `Right Arrow` key
#define XD_RL_ANSI_LEFT_ARROW  "\033[D"  // ANSI for `Left Arrow` key

#define XD_RL_ANSI_PAGE_UP   "\033[5~"  // ANSI for `Page Up` key
#define XD_RL_ANSI_PAGE_DOWN "\033[6~"  // ANSI for `Page Down` key
#define XD_RL_ANSI_HOME      "\033[H"   // ANSI for `Home` key
#define XD_RL_ANSI_END       "\033[F"   // ANSI for `End` key
#define XD_RL_ANSI_DELETE    "\033[3~"  // ANSI for `Delete` key

#define XD_RL_ANSI_ALT_F "\033f"  // ANSI for `ALT+F` key
#define XD_RL_ANSI_ALT_B "\033b"  // ANSI for `ALT+B` key
#define XD_RL_ANSI_ALT_D "\033d"  // ANSI for `ALT+D` key

#define XD_RL_ANSI_ALT_BS "\033\177"  // ANSI for `ALT+Backspace` key

#define XD_RL_ANSI_CTRL_UARROW "\033[1;5A"  // ANSI for `Ctrl+Up Arrow` key
#define XD_RL_ANSI_CTRL_DARROW "\033[1;5B"  // ANSI for `Ctrl+Down Arrow` key
#define XD_RL_ANSI_CTRL_RARROW "\033[1;5C"  // ANSI for `Ctrl+Right Arrow` key
#define XD_RL_ANSI_CTRL_LARROW "\033[1;5D"  // ANSI for `Ctrl+Left Arrow` key

#define XD_RL_ANSI_CTRL_PAGE_UP "\033[5;5~"  //  ANSI for `Ctrl+Page Up` key
#define XD_RL_ANSI_CTRL_PAGE_DN "\033[6;5~"  //  ANSI for `Ctrl+Page Down` key
#define XD_RL_ANSI_CTRL_DELETE  "\033[3;5~"  // ANSI for `Ctrl+Delete` key

// ANSI sequences' formats

#define XD_RL_ANSI_CRSR_SET_COL "\033[%dG"   // ANSI for setting cursor column
#define XD_RL_ANSI_CRSR_MV_HOME "\033[H"     // ANSI for moving cursor to (1, 1)
#define XD_RL_ANSI_CRSR_MV_UP   "\033[%dA"   // ANSI for moving cursor up
#define XD_RL_ANSI_CRSR_MV_DN   "\033[%dB"   // ANSI for moving cursor down
#define XD_RL_ANSI_LINE_CLR     "\033[2K\r"  // ANSI for clearing current line
#define XD_RL_ANSI_SCRN_CLR     "\033[2J"    // ANSI for clearing the screen

#define XD_RL_ANSI_CRSR_REQ_POS "\033[6n"  // ANSI for requesting crsr position

#define XD_RL_ANSI_TEXT_HIGHLIGHT "\033[30;107m"  // ANSI for text highlight
#define XD_RL_ANSI_TEXT_RESET     "\033[0m"       // ANSI for text restore

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

/**
 * @brief Represents the running mode of `xd_readline`.
 */
typedef enum xd_readline_mode_t {
  XD_READLINE_NORMAL,
  XD_READLINE_REVERSE_SEARCH,
  XD_READLINE_FORWARD_SEARCH,
} xd_readline_mode_t;

// ========================
// Function Declarations
// ========================

static char *xd_util_longest_common_prefix(const char **strings);
static void xd_util_print_completions(char **completions);
static const char *xd_util_base_name_keep_trailing_slash(const char *path);

static void xd_readline_init() __attribute__((constructor));
static void xd_readline_destroy() __attribute__((destructor));

static void xd_readline_history_init();
static void xd_readline_history_destroy();

static void xd_input_buffer_insert(char chr);
static void xd_input_buffer_insert_string(const char *str);
static void xd_input_buffer_remove_before_cursor(int n);
static void xd_input_buffer_remove_from_cursor(int n);

static int xd_input_buffer_get_current_word_end();
static int xd_input_buffer_get_current_word_start();

static void xd_input_buffer_save_to_history();
static void xd_input_buffer_load_from_history();

static void xd_tty_raw();
static void xd_tty_restore();

static void xd_tty_cursor_fix_initial_pos();

static inline void xd_tty_bell();

static void xd_tty_input_clear();
static void xd_tty_input_redraw();

static void xd_tty_screen_resize();

static void xd_tty_write_ansii_sequence(const char *format, ...);
static void xd_tty_write(const void *data, int length);
static void xd_tty_write_track(const void *data, int length);
static void xd_tty_write_colored_track(const void *data, int length);

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
static void xd_input_handle_ctrl_r();
static void xd_input_handle_ctrl_s();
static void xd_input_handle_ctrl_u();

static void xd_input_handle_tab();

static void xd_input_handle_backspace();
static void xd_input_handle_enter();

static void xd_input_handle_up_arrow();
static void xd_input_handle_down_arrow();
static void xd_input_handle_right_arrow();
static void xd_input_handle_left_arrow();

static void xd_input_handle_page_up();
static void xd_input_handle_page_down();
static void xd_input_handle_home();
static void xd_input_handle_end();
static void xd_input_handle_delete();

static void xd_input_handler_ctrl_up_arrow();
static void xd_input_handler_ctrl_down_arrow();
static void xd_input_handle_ctrl_right_arrow();
static void xd_input_handle_ctrl_left_arrow();

static void xd_input_handler_ctrl_page_up();
static void xd_input_handler_ctrl_page_down();
static void xd_input_handle_ctrl_delete();

static void xd_input_handle_alt_f();
static void xd_input_handle_alt_b();
static void xd_input_handle_alt_d();
static void xd_input_handle_alt_backspace();

static void xd_input_handle_escape_sequence();

static void xd_input_handle_control(char chr);

static void xd_input_handler(char chr);

static void xd_readline_history_reverse_search();
static void xd_readline_history_forward_search();

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
 * @brief The previous char read from `stdin` using `read()`.
 */
static char xd_readline_prev_read_char = XD_RL_ASCII_NUL;

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
static int xd_history_nav_idx = XD_RL_HISTORY_MAX;

/**
 * @brief Index of the first history entry.
 */
static int xd_history_start_idx = 0;

/**
 * @brief Index of the last history entry.
 */
static int xd_history_end_idx = XD_RL_HISTORY_MAX - 1;

/**
 * @brief The number of entries currently stored in history.
 */
static int xd_history_length = 0;

/**
 * @brief The current running mode of `xd_readline`.
 */
static xd_readline_mode_t xd_readline_mode = XD_READLINE_NORMAL;

/**
 * @brief History search prompt.
 */
static const char *xd_search_prompt = "";

/**
 * @brief History search query.
 */
static char *xd_search_query_buffer = NULL;

/**
 * @brief History search query length.
 */
static int xd_search_query_length = 0;

/**
 * @brief History index used while searching.
 */
static int xd_search_idx = XD_RL_SEARCH_IDX_NEW;

/**
 * @brief History navigation index before starting history search.
 */
static int xd_search_original_nav_idx = 0;

/**
 * @brief Input cursor before starting history search.
 */
static int xd_search_original_input_cursor = 0;

/**
 * @brief Start position of search result (current input) highlight.
 */
static int xd_search_result_highlight_start = -1;

/**
 * @brief Array mapping ANSI escape sequences to corresponding input
 * handlers.
 */
static const xd_esc_seq_binding_t xd_esc_seq_bindings[] = {
    {XD_RL_ANSI_UP_ARROW,     xd_input_handle_up_arrow        },
    {XD_RL_ANSI_DOWN_ARROW,   xd_input_handle_down_arrow      },
    {XD_RL_ANSI_RIGHT_ARROW,  xd_input_handle_right_arrow     },
    {XD_RL_ANSI_LEFT_ARROW,   xd_input_handle_left_arrow      },
    {XD_RL_ANSI_PAGE_UP,      xd_input_handle_page_up         },
    {XD_RL_ANSI_PAGE_DOWN,    xd_input_handle_page_down       },
    {XD_RL_ANSI_HOME,         xd_input_handle_home            },
    {XD_RL_ANSI_END,          xd_input_handle_end             },
    {XD_RL_ANSI_DELETE,       xd_input_handle_delete          },
    {XD_RL_ANSI_ALT_F,        xd_input_handle_alt_f           },
    {XD_RL_ANSI_ALT_B,        xd_input_handle_alt_b           },
    {XD_RL_ANSI_ALT_D,        xd_input_handle_alt_d           },
    {XD_RL_ANSI_ALT_BS,       xd_input_handle_alt_backspace   },
    {XD_RL_ANSI_CTRL_UARROW,  xd_input_handler_ctrl_up_arrow  },
    {XD_RL_ANSI_CTRL_DARROW,  xd_input_handler_ctrl_down_arrow},
    {XD_RL_ANSI_CTRL_RARROW,  xd_input_handle_ctrl_right_arrow},
    {XD_RL_ANSI_CTRL_LARROW,  xd_input_handle_ctrl_left_arrow },
    {XD_RL_ANSI_CTRL_PAGE_UP, xd_input_handler_ctrl_page_up   },
    {XD_RL_ANSI_CTRL_PAGE_DN, xd_input_handler_ctrl_page_down },
    {XD_RL_ANSI_CTRL_DELETE,  xd_input_handle_ctrl_delete     },
};

/**
 * @brief Number of defined escape sequence bindings.
 */
static const int xd_esc_seq_bindings_length =
    sizeof(xd_esc_seq_bindings) / sizeof(xd_esc_seq_bindings[0]);

// ========================
// Public Variables
// ========================

xd_readline_completion_gen_func_t xd_readline_completions_generator = NULL;

const char *xd_readline_prompt = NULL;

// ========================
// Function Definitions
// ========================

/**
 * @brief Returns the longest common prefix of the passed array of strings.
 *
 * @param strings Sorted, null-terminated array of strings.
 *
 * @return Pointer to a newly allocated string containing the longest common
 * prefix, or `NULL` if the passed array is `NULL` or empty or on allocation
 * failure.
 */
static char *xd_util_longest_common_prefix(const char **strings) {
  if (strings == NULL || strings[0] == NULL) {
    return NULL;
  }
  const char *first_str = strings[0];
  int lcp_length = 0;
  int done = 0;
  while (!done) {
    char chr = first_str[lcp_length];
    if (chr == XD_RL_ASCII_NUL) {
      break;
    }
    for (int i = 1; strings[i] != XD_RL_ASCII_NUL; i++) {
      if (strings[i][lcp_length] != chr) {
        done = 1;
        break;
      }
    }
    if (!done) {
      lcp_length++;
    }
  }
  return lcp_length == 0 ? NULL : strndup(first_str, lcp_length);
}  // xd_util_longest_common_prefix()

/**
 * @brief Helper used to print all possible completions when the `Tab` key is
 * pressed more than once and there is more than one completion.
 *
 * @param completions Sorted, null-terminated array of possible completions.
 */
static void xd_util_print_completions(char **completions) {
  if (completions == NULL || *completions == NULL) {
    return;
  }
  // restore original terminal settings so we can use printf
  xd_tty_restore();

  int longest_completion_length = 0;
  int completions_count = 0;
  for (int i = 0; completions[i] != NULL; i++) {
    if ((int)strlen(completions[i]) > longest_completion_length) {
      longest_completion_length = (int)strlen(completions[i]);
    }
    completions_count++;
  }

  // calculate the number of rows and columns
  int col_length = longest_completion_length + 2;
  if (col_length > xd_tty_win_width) {
    col_length = xd_tty_win_width;
  }
  int col_count = xd_tty_win_width / col_length;
  int row_count = (completions_count + col_count - 1) / col_count;

  // print completions
  printf("\n");
  for (int row = 0; row < row_count; row++) {
    for (int col = 0; col < col_count; col++) {
      int idx = row + (col * row_count);
      if (idx < completions_count) {
        const char *basename =
            xd_util_base_name_keep_trailing_slash(completions[idx]);
        printf("%-*s", col_length, basename);
      }
    }
    printf("\n");
  }

  // change the terminal settings back to raw
  xd_tty_raw();

  // start fresh on new line
  xd_tty_cursor_row = 1;
  xd_tty_cursor_col = 1;
  xd_tty_chars_count = 0;
  xd_readline_redraw = 1;
}  // xd_util_print_completions()

/**
 * @brief Returns a pointer to the last segment of the passed path.
 *
 * @param path The path to return its last segment.
 *
 * @return Pointer to the beginning of the last segment.
 */
static const char *xd_util_base_name_keep_trailing_slash(const char *path) {
  if (path == NULL || *path == '\0') {
    return path;
  }

  int path_length = (int)strlen(path);
  const char *last_slash = NULL;
  for (int i = path_length - 1; i >= 0; i--) {
    if (path[i] == '/' && i != path_length - 1) {
      last_slash = path + i + 1;
      break;
    }
  }
  return last_slash == NULL ? path : last_slash;
}  // xd_util_base_name_keep_trailing_slash()

/**
 * @brief Constructor, runs before main to initialize the `xd-readline`
 * library.
 */
static void xd_readline_init() {
  if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
    return;
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
  xd_input_buffer[0] = XD_RL_ASCII_NUL;

  // initialize search query buffer
  xd_search_query_buffer =
      (char *)malloc(sizeof(char) * XD_RL_SEARCH_QUERY_MAX);
  if (xd_search_query_buffer == NULL) {
    fprintf(stderr, "xd_readline: failed to allocate memory: %s\n",
            strerror(errno));
    exit(EXIT_FAILURE);
  }
  xd_search_query_length = 0;
  xd_search_query_buffer[0] = XD_RL_ASCII_NUL;

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
  if (xd_input_buffer == NULL) {
    return;
  }
  xd_readline_history_destroy();
  free(xd_input_buffer);
  free(xd_search_query_buffer);
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
                                             (XD_RL_HISTORY_MAX + 1));
  if (xd_history == NULL) {
    fprintf(stderr, "xd_readline: failed to allocate memory: %s\n",
            strerror(errno));
    exit(EXIT_FAILURE);
  }

  for (int i = 0; i <= XD_RL_HISTORY_MAX; i++) {
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
    xd_history[i]->str[0] = XD_RL_ASCII_NUL;
  }
}  // xd_readline_history_init()

/**
 * @brief Frees the resources used for the history.
 */
static void xd_readline_history_destroy() {
  for (int i = 0; i <= XD_RL_HISTORY_MAX; i++) {
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
  if (chr == XD_RL_ASCII_NUL) {
    return;
  }
  // shift all the characters starting from the cursor by one to the right
  for (int i = xd_input_length; i > xd_input_cursor; i--) {
    xd_input_buffer[i] = xd_input_buffer[i - 1];
  }
  // insert the new character
  xd_input_buffer[xd_input_cursor++] = chr;
  xd_input_buffer[++xd_input_length] = XD_RL_ASCII_NUL;
}  // xd_input_buffer_insert()

/**
 * @brief Inserts the characters of the passed string into the input buffer at
 * the cursor position.
 *
 * @param str The string to be inserted.
 */
static void xd_input_buffer_insert_string(const char *str) {
  if (str == NULL || str[0] == XD_RL_ASCII_NUL) {
    return;
  }
  for (int i = 0; str[i] != XD_RL_ASCII_NUL; i++) {
    xd_input_buffer_insert(str[i]);
  }
}  // xd_input_buffer_insert_string()

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
  xd_input_buffer[xd_input_length] = XD_RL_ASCII_NUL;
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
  xd_input_buffer[xd_input_length] = XD_RL_ASCII_NUL;
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

    char *ptr =
        (char *)realloc(history_entry->str, sizeof(char) * new_capacity);
    if (ptr == NULL) {
      return;  // allocation error, stop saving
    }
    history_entry->capacity = new_capacity;
    history_entry->str = ptr;
  }

  memcpy(history_entry->str, xd_input_buffer, xd_input_length);
  history_entry->str[xd_input_length] = XD_RL_ASCII_NUL;
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

    char *ptr = (char *)realloc(xd_input_buffer, sizeof(char) * new_capacity);
    if (ptr == NULL) {
      return;  // allocation error, stop loading
    }
    xd_input_capacity = new_capacity;
    xd_input_buffer = ptr;
  }

  xd_input_length = history_entry->length;
  xd_input_cursor = xd_input_length;
  memcpy(xd_input_buffer, history_entry->str, xd_input_length);
  xd_input_buffer[xd_input_length] = XD_RL_ASCII_NUL;
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
  while (tcsetattr(STDIN_FILENO, TCSANOW, &xd_getline_tty_attributes) == -1) {
    if (errno == EINTR) {
      continue;
    }
    fprintf(stderr, "xd_readline: failed to set tty attributes\n");
    exit(EXIT_FAILURE);
  }
}  // xd_tty_raw()

/**
 * @brief Restore original terminal settings.
 */
static void xd_tty_restore() {
  while (tcsetattr(STDIN_FILENO, TCSANOW, &xd_original_tty_attributes) == -1) {
    if (errno == EINTR) {
      continue;
    }
    fprintf(stderr, "xd_readline: failed to reset tty attributes\n");
    exit(EXIT_FAILURE);
  }
}  // xd_tty_restore()

/**
 * @brief Helper used to ensures the tty cursor is on a fresh new line first
 * thing after calling `xd_readline()`.
 */
static void xd_tty_cursor_fix_initial_pos() {
  xd_tty_write_ansii_sequence(XD_RL_ANSI_CRSR_REQ_POS);
  tcdrain(STDOUT_FILENO);

  char buf[XD_RL_SMALL_BUFFER_SIZE];
  int idx = 0;
  char chr = ' ';
  while (idx < XD_RL_SMALL_BUFFER_SIZE - 1) {
    ssize_t ret = read(STDIN_FILENO, &chr, 1);
    if (ret <= 0) {
      break;
    }
    buf[idx++] = chr;
    if (chr == 'R') {
      break;
    }
  }
  buf[idx] = '\0';

  int row = 1;
  int col = 1;

  if (sscanf(buf, "\033[%d;%dR", &row, &col) == 2 && col != 1) {
    // move to new line to preserve text on the same line
    xd_tty_write("\r\n", 2);
  }
}  // xd_tty_cursor_fix_initial_pos()

/**
 * @brief Write bell character to terminal to make an alert sound.
 */
static inline void xd_tty_bell() {
  char chr = XD_RL_ASCII_BEL;
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
    xd_tty_write_ansii_sequence(XD_RL_ANSI_LINE_CLR);
    xd_tty_cursor_col = 1;
    if (i < rows - 1) {
      xd_tty_write_ansii_sequence(XD_RL_ANSI_CRSR_MV_UP, 1);
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
  if (xd_readline_mode == XD_READLINE_NORMAL) {
    xd_tty_write_colored_track(xd_readline_prompt, xd_readline_prompt_length);
    xd_tty_write_track(xd_input_buffer, xd_input_length);
  }
  else {
    // search mode
    xd_tty_write_track(xd_search_prompt, (int)strlen(xd_search_prompt));
    xd_tty_write_track("'", 1);
    xd_tty_write_track(xd_search_query_buffer, xd_search_query_length);
    xd_tty_write_track("': ", 3);
    if (xd_search_result_highlight_start != -1) {
      char *input_hstart = xd_input_buffer + xd_search_result_highlight_start;
      char *input_hend = xd_input_buffer + xd_search_result_highlight_start +
                         xd_search_query_length;
      int after_hlength = xd_input_length - xd_search_result_highlight_start -
                          xd_search_query_length;
      xd_tty_write_track(xd_input_buffer, xd_search_result_highlight_start);
      xd_tty_write_ansii_sequence(XD_RL_ANSI_TEXT_HIGHLIGHT);
      xd_tty_write_track(input_hstart, xd_search_query_length);
      xd_tty_write_ansii_sequence(XD_RL_ANSI_TEXT_RESET);
      xd_tty_write_track(input_hend, after_hlength);
    }
    else {
      xd_tty_write_track(xd_input_buffer, xd_input_length);
    }
    if (strcmp(xd_search_prompt, XD_RL_REVERSE_SEARCH_PROMPT_FAILED) == 0) {
      xd_tty_bell();
    }
  }
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
  char buffer[XD_RL_SMALL_BUFFER_SIZE] = {0};
  va_list args;
  va_start(args, format);
  int length = vsnprintf(buffer, XD_RL_SMALL_BUFFER_SIZE, format, args);
  va_end(args);
  write(STDOUT_FILENO, buffer, length);
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
  write(STDOUT_FILENO, data, length);
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

  int written = (int)write(STDOUT_FILENO, data, length);
  if (written == -1) {
    return;
  }
  xd_tty_chars_count += written;

  // update cursor position
  int cursor_flat_pos = ((xd_tty_cursor_row - 1) * xd_tty_win_width) +
                        xd_tty_cursor_col + written - 1;
  xd_tty_cursor_row = (cursor_flat_pos / xd_tty_win_width) + 1;
  xd_tty_cursor_col = (cursor_flat_pos % xd_tty_win_width) + 1;
  if (xd_tty_cursor_col == 1) {
    // make the terminal wrap to new line
    char chr = ' ';
    xd_tty_write(&chr, 1);
  }
  xd_tty_write_ansii_sequence(XD_RL_ANSI_CRSR_SET_COL, xd_tty_cursor_col);
}  // xd_tty_write_track()

/**
 * @brief Writes the passed data to the terminal. It identifies and handles ANSI
 * color codes starting with `\033[` and ending with `m` and writes them
 * directly to the terminal without affecting the cursor tracking logic.
 *
 * @param data Pointer to the data to be written.
 * @param length The number of bytes to be written.
 */
static void xd_tty_write_colored_track(const void *data, int length) {
  if (length <= 0) {
    return;
  }
  const char *str = data;
  int str_idx = 0;
  while (str_idx < length) {
    if (str[str_idx] == '\033') {
      int ansii_end_idx = str_idx;
      while (ansii_end_idx < length && str[ansii_end_idx] != 'm') {
        ansii_end_idx++;
      }
      if (ansii_end_idx < length && str[ansii_end_idx] == 'm') {
        // valid ANSI sequence, write without updating cursor
        xd_tty_write(str + str_idx, ansii_end_idx - str_idx + 1);
        str_idx = ansii_end_idx + 1;
      }
      continue;
    }
    xd_tty_write_track(str + str_idx, 1);
    str_idx++;
  }
}  // xd_tty_write_colored_track()

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
    xd_tty_write_ansii_sequence(XD_RL_ANSI_CRSR_MV_UP,
                                xd_tty_cursor_row - new_cursor_row);
    xd_tty_cursor_row = new_cursor_row;
  }
  xd_tty_write_ansii_sequence(XD_RL_ANSI_CRSR_SET_COL, new_cursor_col);
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
    xd_tty_write_ansii_sequence(XD_RL_ANSI_CRSR_MV_DN,
                                new_cursor_row - xd_tty_cursor_row);
    xd_tty_cursor_row = new_cursor_row;
  }
  xd_tty_write_ansii_sequence(XD_RL_ANSI_CRSR_SET_COL, new_cursor_col);
  xd_tty_cursor_col = new_cursor_col;
}  // xd_tty_cursor_move_right_wrap()

/**
 * @brief Handles the case where the input is a printable character.
 *
 * Adds the character to the input at the cursor position, or if in history
 * search mode, adds it to the end of the search query.
 *
 * @param chr the input character.
 */
static void xd_input_handle_printable(char chr) {
  if (xd_readline_mode == XD_READLINE_NORMAL) {
    xd_input_buffer_insert(chr);
    if (xd_input_cursor == xd_input_length) {
      xd_tty_write_track(&chr, 1);
      // don't redraw when adding to the end
      return;
    }
    xd_readline_redraw = 1;
  }
  else if (xd_search_query_length < XD_RL_SEARCH_QUERY_MAX - 1) {
    // search mode
    xd_search_query_buffer[xd_search_query_length++] = chr;
    xd_search_query_buffer[xd_search_query_length] = XD_RL_ASCII_NUL;
    xd_search_idx = xd_history_nav_idx;  // reset search index
    xd_readline_redraw = 1;
  }
}  // xd_input_handle_printable()

/**
 * @brief Handles the case where the input is `Ctrl+A`.
 *
 * Moves the cursor to the beginning of the input.
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
 *
 * Moves the cursor backward by one character.
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
 *
 * Removes the character at the cursor position, or if the input is empty it
 * emulates `EOF` by making `xd_readline()` stop reading and return `NULL`.
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
 *
 * Moves the cursor to the end of the input.
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
 *
 * Moves the cursor forward by one character.
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
 *
 * Makes a terminal bell sound.
 */
static void xd_input_handle_ctrl_g() {
  xd_tty_bell();
}  // xd_input_handle_ctrl_g()

/**
 * @brief Handles the case where the input is `Ctrl+H`.
 *
 * Removes one character before the cursor from the input, or from the search
 * query if in forward/reverse history search mode.
 */
static void xd_input_handle_ctrl_h() {
  if (xd_readline_mode == XD_READLINE_NORMAL) {
    if (xd_input_cursor == 0) {
      xd_tty_bell();
      return;
    }
    xd_input_buffer_remove_before_cursor(1);
  }
  else if (xd_search_query_length > 0) {
    // search mode
    xd_search_query_buffer[--xd_search_query_length] = XD_RL_ASCII_NUL;
    xd_search_idx = xd_history_nav_idx;  // reset search index
  }
  xd_readline_redraw = 1;
}  // xd_input_handle_ctrl_h()

/**
 * @brief Handles the case where the input is `Ctrl+K`.
 *
 * Removes all characters from the cursor to the end of input.
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
 *
 * Clears the screen.
 */
static void xd_input_handle_ctrl_l() {
  xd_tty_write_ansii_sequence(XD_RL_ANSI_SCRN_CLR);
  xd_tty_write_ansii_sequence(XD_RL_ANSI_CRSR_MV_HOME);
  xd_tty_cursor_row = 1;
  xd_tty_cursor_col = 1;
  xd_readline_redraw = 1;
}  // xd_input_handle_ctrl_l()

/**
 * @brief Handles the case where the input is `Ctrl+R`.
 *
 * Starts history reverse search mode. If already in reverse search, it moves
 * the search index forward by one in order to look for the next match when
 * `xd_readline_history_reverse_search()` is called next.
 */
static void xd_input_handle_ctrl_r() {
  if (xd_readline_mode == XD_READLINE_REVERSE_SEARCH) {
    if (xd_history_nav_idx == xd_history_start_idx) {
      xd_search_idx = XD_RL_SEARCH_IDX_OUT_OF_BOUNDS;
    }
    else if (xd_search_idx == XD_RL_HISTORY_MAX) {
      xd_search_idx = xd_history_end_idx;
    }
    else if (xd_search_idx != XD_RL_SEARCH_IDX_OUT_OF_BOUNDS) {
      xd_search_idx =
          (xd_search_idx - 1 + XD_RL_HISTORY_MAX) % XD_RL_HISTORY_MAX;
    }
    return;
  }

  if (xd_readline_mode == XD_READLINE_NORMAL) {
    xd_input_buffer_save_to_history();
    xd_search_original_nav_idx = xd_history_nav_idx;
    xd_search_original_input_cursor = xd_input_cursor;
    xd_search_query_length = 0;
    xd_search_query_buffer[0] = XD_RL_ASCII_NUL;
    xd_search_idx = XD_RL_SEARCH_IDX_NEW;
  }
  else {
    // switching from forward search
    xd_search_idx = xd_history_nav_idx;
  }
  xd_search_prompt = XD_RL_REVERSE_SERACH_PROMPT;
  xd_readline_mode = XD_READLINE_REVERSE_SEARCH;
  xd_readline_redraw = 1;
}  // xd_input_handle_ctrl_r()

/**
 * @brief Handles the case where the input is `Ctrl+S`.
 *
 * Starts history forward search mode. If already in forward search, it moves
 * the search index forward by one in order to look for the next match when
 * `xd_readline_history_forward_search()` is called next.
 */
static void xd_input_handle_ctrl_s() {
  if (xd_readline_mode == XD_READLINE_FORWARD_SEARCH) {
    if (xd_history_nav_idx == XD_RL_HISTORY_MAX) {
      xd_search_idx = XD_RL_SEARCH_IDX_OUT_OF_BOUNDS;
    }
    else if (xd_search_idx == xd_history_end_idx) {
      xd_search_idx = XD_RL_HISTORY_MAX;
    }
    else if (xd_search_idx != XD_RL_SEARCH_IDX_OUT_OF_BOUNDS) {
      xd_search_idx = (xd_search_idx + 1) % XD_RL_HISTORY_MAX;
    }
    return;
  }

  if (xd_readline_mode == XD_READLINE_NORMAL) {
    xd_input_buffer_save_to_history();
    xd_search_original_nav_idx = xd_history_nav_idx;
    xd_search_original_input_cursor = xd_input_cursor;
    xd_search_query_length = 0;
    xd_search_query_buffer[0] = XD_RL_ASCII_NUL;
    xd_search_idx = XD_RL_SEARCH_IDX_NEW;
  }
  else {
    // switching from reverse search
    xd_search_idx = xd_history_nav_idx;
  }
  xd_search_prompt = XD_RL_FORWARD_SERACH_PROMPT;
  xd_readline_mode = XD_READLINE_FORWARD_SEARCH;
  xd_readline_redraw = 1;
}  // xd_input_handle_ctrl_s()

/**
 * @brief Handles the case where the input is `Ctrl+L`.
 *
 * Removes all characters from the beginning of input to before the cursor.
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
 * @brief Handles the case where the input is the `Tab` key.
 *
 * Attempts to complete the word being written, or if pressed twice in a row it
 * prints all possible completions.
 */
static void xd_input_handle_tab() {
  if (xd_readline_completions_generator == NULL) {
    // completions generator function not set, `Tab` completion won't work
    return;
  }

  // get the current word
  int idx = xd_input_cursor;
  while (idx > 0 &&
         strchr(XD_RL_TAB_COMP_DELIMITERS, xd_input_buffer[idx - 1]) == NULL) {
    idx--;
  }
  int word_length = xd_input_cursor - idx;

  // generate possible completions
  char **completions =
      xd_readline_completions_generator(xd_input_buffer, idx, xd_input_cursor);
  if (completions == NULL) {
    xd_tty_bell();
    return;
  }

  if (completions[0] != NULL && completions[1] == NULL) {
    // single match, replace the word with the match
    xd_input_buffer_insert_string(completions[0] + word_length);
    if (xd_input_buffer[xd_input_cursor - 1] != '/') {
      // add space if it is not a directory
      xd_input_buffer_insert(' ');
    }
  }
  else {
    // multiple matches, replace the word with the longest common prefix
    char *lcp = xd_util_longest_common_prefix((const char **)completions);
    if (lcp != NULL && *(lcp + word_length) != XD_RL_ASCII_NUL) {
      xd_input_buffer_insert_string(lcp + word_length);
    }
    else if (xd_readline_prev_read_char == XD_RL_ASCII_HT) {
      xd_util_print_completions(completions);
    }
    free(lcp);
    xd_tty_bell();
  }

  // free the array of completions
  for (int i = 0; completions[i] != NULL; i++) {
    free(completions[i]);
  }
  free((void *)completions);
  xd_readline_redraw = 1;
}  // xd_input_handle_tab()

/**
 * @brief Handles the case where the input is the`Backspace` key.
 *
 * Removes one character before the cursor from the input, or from the search
 * query if in forward/reverse history search mode.
 */
static void xd_input_handle_backspace() {
  xd_input_handle_ctrl_h();
}  // xd_input_handle_backspace()

/**
 * @brief Handles the case where the input is the `Enter` key.
 *
 * Finalizes the input line by adding new-line character to the end of the input
 * buffer, and  making `xd_readline()` stop reading and return the read line.
 */
static void xd_input_handle_enter() {
  xd_input_buffer[xd_input_length++] = XD_RL_ASCII_LF;
  xd_input_buffer[xd_input_length] = XD_RL_ASCII_NUL;
  xd_readline_finished = 1;
  xd_tty_cursor_move_right_wrap(xd_input_length - xd_input_cursor - 1);
}  // xd_input_handle_enter()

/**
 * @brief Handles the case where the input is the `Up Arrow` key.
 *
 * Moves backward in history by one.
 */
static void xd_input_handle_up_arrow() {
  if (xd_history_length == 0 || xd_history_nav_idx == xd_history_start_idx) {
    xd_tty_bell();
    return;
  }

  xd_input_buffer_save_to_history();
  if (xd_history_nav_idx == XD_RL_HISTORY_MAX) {
    xd_history_nav_idx = xd_history_end_idx;
  }
  else {
    xd_history_nav_idx =
        (xd_history_nav_idx - 1 + XD_RL_HISTORY_MAX) % XD_RL_HISTORY_MAX;
  }
  xd_input_buffer_load_from_history();
  xd_readline_redraw = 1;
}  // xd_input_handle_up_arrow()

/**
 * @brief Handles the case where the input is the `Down Arrow` key.
 *
 * Moves forward in history by one.
 */
static void xd_input_handle_down_arrow() {
  if (xd_history_length == 0 || xd_history_nav_idx == XD_RL_HISTORY_MAX) {
    xd_tty_bell();
    return;
  }

  xd_input_buffer_save_to_history();
  if (xd_history_nav_idx == xd_history_end_idx) {
    xd_history_nav_idx = XD_RL_HISTORY_MAX;
  }
  else {
    xd_history_nav_idx = (xd_history_nav_idx + 1) % XD_RL_HISTORY_MAX;
  }
  xd_input_buffer_load_from_history();
  xd_readline_redraw = 1;
}  // xd_input_handle_down_arrow()

/**
 * @brief Handles the case where the input is the `Right Arrow` key.
 *
 * Moves the cursor forward by one character.
 */
static void xd_input_handle_right_arrow() {
  xd_input_handle_ctrl_f();
}  // xd_input_handle_right_arrow()

/**
 * @brief Handles the case where the input is the `Left Arrow` key.
 *
 * Moves the cursor backward by one character.
 */
static void xd_input_handle_left_arrow() {
  xd_input_handle_ctrl_b();
}  // xd_input_handle_left_arrow()

/**
 * @brief Handles the case where the input is the `Page Up` key.
 *
 * Moves backward in history by one.
 */
static void xd_input_handle_page_up() {
  xd_input_handle_up_arrow();
}  // xd_input_handle_page_up()

/**
 * @brief Handles the case where the input is the `Page Down` key.
 *
 * Moves forward in history by one.
 */
static void xd_input_handle_page_down() {
  xd_input_handle_down_arrow();
}  // xd_input_handle_page_down()

/**
 * @brief Handles the case where the input is the `Home` key.
 *
 * Moves the cursor to the beginning of the input.
 */
static void xd_input_handle_home() {
  xd_input_handle_ctrl_a();
}  // xd_input_handle_home()

/**
 * @brief Handles the case where the input is the `End` key.
 *
 * Moves the cursor to the end of the input.
 */
static void xd_input_handle_end() {
  xd_input_handle_ctrl_e();
}  // xd_input_handle_end()

/**
 * @brief Handles the case where the input is the `Delete` key.
 *
 * Removes the character at the cursor position.
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
 * @brief Handles the case where the input is `Ctrl+Up Arrow`.
 *
 * Moves in history to the first entry.
 */
static void xd_input_handler_ctrl_up_arrow() {
  if (xd_history_length == 0 || xd_history_nav_idx == xd_history_start_idx) {
    xd_tty_bell();
    return;
  }

  xd_input_buffer_save_to_history();
  xd_history_nav_idx = xd_history_start_idx;
  xd_input_buffer_load_from_history();
  xd_readline_redraw = 1;
}  // xd_input_handler_ctrl_up_arrow()

/**
 * @brief Handles the case where the input is `Ctrl+Down Arrow`.
 *
 * Moves in history to the last entry.
 */
static void xd_input_handler_ctrl_down_arrow() {
  if (xd_history_length == 0 || xd_history_nav_idx == XD_RL_HISTORY_MAX) {
    xd_tty_bell();
    return;
  }

  xd_input_buffer_save_to_history();
  xd_history_nav_idx = XD_RL_HISTORY_MAX;
  xd_input_buffer_load_from_history();
  xd_readline_redraw = 1;
}  // xd_input_handler_ctrl_down_arrow()

/**
 * @brief Handles the case where the input is `Ctrl+Right Arrow`.
 *
 * Moves the cursor forward by one word.
 */
static void xd_input_handle_ctrl_right_arrow() {
  xd_input_handle_alt_f();
}  // xd_input_handle_ctrl_right_arrow()

/**
 * @brief Handles the case where the input is `Ctrl+Left Arrow`.
 *
 * Moves the cursor backward by one word.
 */
static void xd_input_handle_ctrl_left_arrow() {
  xd_input_handle_alt_b();
}  // xd_input_handle_ctrl_left_arrow()

/**
 * @brief Handles the case where the input is `Ctrl+Page Up`.
 *
 * Moves in history to the first entry.
 */
static void xd_input_handler_ctrl_page_up() {
  xd_input_handler_ctrl_up_arrow();
}  // xd_input_handler_ctrl_page_up()

/**
 * @brief Handles the case where the input is `Ctrl+Page Down`.
 *
 * Moves in history to the last entry (current input).
 */
static void xd_input_handler_ctrl_page_down() {
  xd_input_handler_ctrl_down_arrow();
}  // xd_input_handler_ctrl_page_down()

/**
 * @brief Handles the case where the input is `Ctrl+Delete`.
 *
 * Removes from the cursor position to the end of the word.
 */
static void xd_input_handle_ctrl_delete() {
  xd_input_handle_alt_d();
}  // xd_input_handle_ctrl_delete()

/**
 * @brief Handles the case where the input is `Alt+F`.
 *
 * Moves the cursor forward by one word.
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
 *
 * Moves the cursor backward by one word.
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
 *
 * Removes from the cursor position to the end of the word.
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
 *
 * Removes from before the cursor position to the beginning of the word.
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
 *
 * Reads the ANSI escape sequence and calls the correct input handler function.
 */
static void xd_input_handle_escape_sequence() {
  // buffer for reading the escape sequence
  int idx = 0;
  char buffer[XD_RL_SMALL_BUFFER_SIZE];
  buffer[idx++] = XD_RL_ASCII_ESC;
  buffer[idx] = XD_RL_ASCII_NUL;

  // read the escape sequence chracters one by one
  char chr;
  int is_valid_prefix = 0;
  while (idx < XD_RL_SMALL_BUFFER_SIZE - 1) {
    if (read(STDIN_FILENO, &chr, 1) != 1) {
      xd_tty_cursor_move_right_wrap(xd_input_length - xd_input_cursor);
      xd_readline_finished = 1;
      xd_readline_return = NULL;
      return;
    }
    buffer[idx++] = chr;
    buffer[idx] = XD_RL_ASCII_NUL;

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
  if (xd_readline_mode != XD_READLINE_NORMAL) {
    if (chr != XD_RL_ASCII_BS && chr != XD_RL_ASCII_DEL &&
        chr != XD_RL_ASCII_DC2 && chr != XD_RL_ASCII_DC3) {
      xd_readline_mode = XD_READLINE_NORMAL;
      xd_readline_redraw = 1;
      if (chr == XD_RL_ASCII_BEL) {
        // `Ctrl+G` restore original input before starting reverse search
        xd_history_nav_idx = xd_search_original_nav_idx;
        xd_input_buffer_load_from_history();
        xd_input_cursor = xd_search_original_input_cursor;
        return;
      }
    }
  }
  switch (chr) {
    case XD_RL_ASCII_SOH:
      xd_input_handle_ctrl_a();
      break;
    case XD_RL_ASCII_STX:
      xd_input_handle_ctrl_b();
      break;
    case XD_RL_ASCII_EOT:
      xd_input_handle_ctrl_d();
      break;
    case XD_RL_ASCII_ENQ:
      xd_input_handle_ctrl_e();
      break;
    case XD_RL_ASCII_ACK:
      xd_input_handle_ctrl_f();
      break;
    case XD_RL_ASCII_BEL:
      xd_input_handle_ctrl_g();
      break;
    case XD_RL_ASCII_BS:
      xd_input_handle_ctrl_h();
      break;
    case XD_RL_ASCII_HT:
      xd_input_handle_tab();
      break;
    case XD_RL_ASCII_LF:
      xd_input_handle_enter();
      break;
    case XD_RL_ASCII_VT:
      xd_input_handle_ctrl_k();
      break;
    case XD_RL_ASCII_FF:
      xd_input_handle_ctrl_l();
      break;
    case XD_RL_ASCII_DC2:
      xd_input_handle_ctrl_r();
      break;
    case XD_RL_ASCII_DC3:
      xd_input_handle_ctrl_s();
      break;
    case XD_RL_ASCII_NAK:
      xd_input_handle_ctrl_u();
      break;
    case XD_RL_ASCII_ESC:
      xd_input_handle_escape_sequence();
      break;
    case XD_RL_ASCII_DEL:
      xd_input_handle_backspace();
      break;
    default:
      break;
  }
}  // xd_input_handle_control()

/**
 * @brief Handles history reverse search.
 */
static void xd_readline_history_reverse_search() {
  if (xd_search_idx == XD_RL_SEARCH_IDX_NEW) {
    xd_search_idx = xd_history_nav_idx;
    return;
  }

  if (xd_search_query_length == 0 ||
      xd_search_idx == XD_RL_SEARCH_IDX_OUT_OF_BOUNDS) {
    xd_search_prompt = XD_RL_REVERSE_SEARCH_PROMPT_FAILED;
    xd_search_result_highlight_start = -1;
    xd_readline_redraw = 1;
    return;
  }

  int max_iterations = xd_history_length + 1;
  const char *res = NULL;
  for (int i = 0; i < max_iterations; i++) {
    res = strstr(xd_history[xd_search_idx]->str, xd_search_query_buffer);
    if (res != NULL || xd_search_idx == xd_history_start_idx) {
      break;
    }
    if (xd_search_idx == XD_RL_HISTORY_MAX) {
      xd_search_idx = xd_history_end_idx;
    }
    else {
      xd_search_idx =
          (xd_search_idx - 1 + XD_RL_HISTORY_MAX) % XD_RL_HISTORY_MAX;
    }
  }

  if (res == NULL) {
    xd_search_prompt = XD_RL_REVERSE_SEARCH_PROMPT_FAILED;
    xd_search_result_highlight_start = -1;
    xd_search_idx = XD_RL_SEARCH_IDX_OUT_OF_BOUNDS;
  }
  else {
    xd_history_nav_idx = xd_search_idx;
    xd_input_buffer_load_from_history();
    xd_search_prompt = XD_RL_REVERSE_SERACH_PROMPT;
    xd_input_cursor = (int)(res - xd_history[xd_search_idx]->str);
    xd_search_result_highlight_start = xd_input_cursor;
  }
  xd_readline_redraw = 1;
}  // xd_readline_history_reverse_search()

/**
 * @brief Handles history reverse search.
 */
static void xd_readline_history_forward_search() {
  if (xd_search_idx == XD_RL_SEARCH_IDX_NEW) {
    xd_search_idx = xd_history_nav_idx;
    return;
  }

  if (xd_search_query_length == 0 ||
      xd_search_idx == XD_RL_SEARCH_IDX_OUT_OF_BOUNDS) {
    xd_search_prompt = XD_RL_FORWARD_SERACH_PROMPT_FAILED;
    xd_search_result_highlight_start = -1;
    xd_readline_redraw = 1;
    return;
  }

  int max_iterations = xd_history_length + 1;
  const char *res = NULL;
  for (int i = 0; i < max_iterations; i++) {
    res = strstr(xd_history[xd_search_idx]->str, xd_search_query_buffer);
    if (res != NULL || xd_search_idx == XD_RL_HISTORY_MAX) {
      break;
    }
    if (xd_search_idx == xd_history_end_idx) {
      xd_search_idx = XD_RL_HISTORY_MAX;
    }
    else {
      xd_search_idx = (xd_search_idx + 1) % XD_RL_HISTORY_MAX;
    }
  }

  if (res == NULL) {
    xd_search_prompt = XD_RL_FORWARD_SERACH_PROMPT_FAILED;
    xd_search_result_highlight_start = -1;
    xd_search_idx = XD_RL_SEARCH_IDX_OUT_OF_BOUNDS;
  }
  else {
    xd_history_nav_idx = xd_search_idx;
    xd_input_buffer_load_from_history();
    xd_search_prompt = XD_RL_FORWARD_SERACH_PROMPT;
    xd_input_cursor = (int)(res - xd_history[xd_search_idx]->str);
    xd_search_result_highlight_start = xd_input_cursor;
  }
  xd_readline_redraw = 1;
}  // xd_readline_history_forward_search()

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
  if (xd_input_buffer == NULL) {
    errno = ENOTTY;
    return NULL;
  }

  if (xd_readline_prompt != NULL) {
    xd_readline_prompt_length = (int)strlen(xd_readline_prompt);
  }
  else {
    xd_readline_prompt_length = 0;
  }

  xd_readline_mode = XD_READLINE_NORMAL;

  xd_input_cursor = 0;
  xd_input_length = 0;
  xd_input_buffer[0] = XD_RL_ASCII_NUL;

  xd_readline_redraw = 1;
  xd_readline_return = xd_input_buffer;
  xd_readline_finished = 0;

  xd_tty_cursor_row = 1;
  xd_tty_cursor_col = 1;
  xd_tty_chars_count = 0;

  xd_history_nav_idx = XD_RL_HISTORY_MAX;

  xd_tty_raw();

  xd_tty_cursor_fix_initial_pos();

  char chr = XD_RL_ASCII_NUL;
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
      char *ptr = (char *)realloc(xd_input_buffer,
                                  sizeof(char) * xd_input_capacity * 2);
      if (ptr == NULL) {
        break;
      }
      xd_input_capacity *= 2;
      xd_input_buffer = ptr;
      xd_readline_return = xd_input_buffer;
    }

    xd_readline_prev_read_char = chr;

    // read one character
    ssize_t ret = read(STDIN_FILENO, &chr, 1);

    // EOF or Error while reading
    if (ret <= 0) {
      xd_tty_cursor_move_right_wrap(xd_input_length - xd_input_cursor);
      xd_readline_finished = 1;
      xd_readline_return = NULL;
      continue;
    }

    xd_input_handler(chr);

    if (xd_readline_mode == XD_READLINE_REVERSE_SEARCH) {
      xd_readline_history_reverse_search();
    }
    else if (xd_readline_mode == XD_READLINE_FORWARD_SEARCH) {
      xd_readline_history_forward_search();
    }
  }

  if (xd_tty_cursor_col != 1) {
    chr = XD_RL_ASCII_LF;
    xd_tty_write(&chr, 1);
  }

  xd_tty_restore();
  return xd_readline_return;
}  // xd_readline()

void xd_readline_history_clear() {
  for (int i = 0; i <= XD_RL_HISTORY_MAX; i++) {
    xd_history[i]->length = 0;
    xd_history[i]->str[0] = XD_RL_ASCII_NUL;
  }
  xd_history_nav_idx = XD_RL_HISTORY_MAX;
  xd_history_start_idx = 0;
  xd_history_end_idx = XD_RL_HISTORY_MAX - 1;
  xd_history_length = 0;
}  // xd_readline_history_clear()

int xd_readline_history_add(const char *str) {
  if (str == NULL) {
    return -1;
  }

  // ignore the trailing newline at the end
  int str_length = (int)strlen(str);
  if (str_length > 0 && str[str_length - 1] == XD_RL_ASCII_LF) {
    str_length--;
  }

  int new_end_idx = (xd_history_end_idx + 1) % XD_RL_HISTORY_MAX;
  xd_history_entry_t *history_entry = xd_history[new_end_idx];

  // resize the history entry string if needed
  if (str_length > history_entry->capacity - 1) {
    // resize to multiple of `LINE_MAX`
    int new_capacity = str_length + 1;
    if (new_capacity % LINE_MAX != 0) {
      new_capacity += LINE_MAX - (new_capacity % LINE_MAX);
    }

    char *ptr =
        (char *)realloc(history_entry->str, sizeof(char) * new_capacity);
    if (ptr == NULL) {
      fprintf(stderr, "xd_readline: failed to allocate memory: %s\n",
              strerror(errno));
      return -1;
    }
    history_entry->capacity = new_capacity;
    history_entry->str = ptr;
  }

  // add to history
  if (xd_history_length < XD_RL_HISTORY_MAX) {
    xd_history_length++;
  }
  else {
    // circular buffer is full, overwrite the oldest entry
    xd_history_start_idx = (xd_history_start_idx + 1) % XD_RL_HISTORY_MAX;
  }
  xd_history_end_idx = new_end_idx;
  memcpy(history_entry->str, str, str_length);
  history_entry->str[str_length] = XD_RL_ASCII_NUL;
  history_entry->length = str_length;

  return 0;
}  // xd_readline_history_add()

char *xd_readline_history_get(int n) {
  if (n == 0 || xd_history_length == 0) {
    return NULL;
  }

  int idx = 0;
  if (n > 0) {
    idx = (xd_history_start_idx + n - 1) % XD_RL_HISTORY_MAX;
  }
  else {
    n = -n;
    idx = (xd_history_end_idx - n + 1 + XD_RL_HISTORY_MAX) % XD_RL_HISTORY_MAX;
  }

  if (n > xd_history_length) {
    return NULL;
  }

  char *ptr = strdup(xd_history[idx]->str);
  if (ptr == NULL) {
    fprintf(stderr, "xd_readline: failed to allocate memory: %s\n",
            strerror(errno));
  }
  return ptr;
}  // xd_readline_history_get()

void xd_readline_history_print() {
  int idx = xd_history_start_idx;
  for (int i = 0; i < xd_history_length; i++) {
    printf("    %d  %s\n", i + 1, xd_history[idx]->str);
    idx = (idx + 1) % XD_RL_HISTORY_MAX;
  }
}  // xd_readline_history_print()

int xd_readline_history_save_to_file(const char *path, int append) {
  const char *open_mode = append == 0 ? "w" : "a";
  FILE *file = fopen(path, open_mode);
  if (file == NULL) {
    return -1;
  }
  int idx = xd_history_start_idx;
  for (int i = 0; i < xd_history_length; i++) {
    fprintf(file, "%s\n", xd_history[idx]->str);
    idx = (idx + 1) % XD_RL_HISTORY_MAX;
  }
  fclose(file);
  return 0;
}  // xd_readline_history_save_to_file()

int xd_readline_history_load_from_file(const char *path) {
  FILE *file = fopen(path, "r");
  if (file == NULL) {
    return -1;
  }
  char *line = NULL;
  size_t size = 0;
  while (getline(&line, &size, file) != -1) {
    xd_readline_history_add(line);
  }
  free(line);
  fclose(file);
  return 0;
}  // xd_readline_history_load_from_file()
