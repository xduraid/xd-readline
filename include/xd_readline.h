/*
 * ==============================================================================
 * File: xd_readline.h
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

#ifndef XD_READLINE_H
#define XD_READLINE_H

/**
 * @brief Maximum number of history entries.
 */
#define XD_RL_HISTORY_MAX (1000)

/**
 * @brief Characters which define the start of the word to be completed when
 * `Tab` key is pressed.
 *
 * The word to be completed will start after one of these characters.
 */
#define XD_RL_TAB_COMP_DELIMITERS "'\"`!*?[]{}()<>~#$`:=;&|@\%^\\ "

/**
 * @brief Function type for the function responsible for generating all possible
 * completions when pressing `Tab`.
 *
 * @param line The whole line being read.
 * @param start Start position of the partial text to be completed within the
 * line.
 * @param end End position of the partial text to be completed within the line.
 *
 * @return A newly-allocated, sorted, and null-terminated string array of
 * possible completions.
 */
typedef char **(*xd_readline_completion_gen_func_t)(const char *line, int start,
                                                    int end);

/**
 * @brief Pointer to the function used for generating all possible completions
 * when pressing `Tab`, if not set then `Tab` completion won't work.
 *
 * Example: generating `"bl"` completions returns `["black.txt", "blue.txt",
 * NULL]`
 *
 * @warning This function will be called within `xd_readline()` where the
 * terminal settings are changed, don't read/write to `stdout` or `stdin` within
 * this function or you will break `xd_readline()`'s correct functionality.
 *
 * @note This function must return a newly allocated null-terminated and sorted
 * array of strings containing all the possible completions for the passed
 * string.
 *
 * @note For path completion, this function must return all possible path
 * completions with the directory completions having '/' at their end.
 */
extern xd_readline_completion_gen_func_t xd_readline_completions_generator;

/**
 * @brief Prompt string displayed at the beginning of each input line.
 *
 * Used to change the prompt displayed by `xd_readline()` before reading input.
 * If not set or `NULL`, no prompt will be displayed.
 *
 * Supports ANSI escape sequences for coloring, only full ANSI SGR sequences
 * starting with `\033[` and ending with `m` are supported.
 */
extern const char *xd_readline_prompt;

/**
 * @brief Reads a line from standard input with custom editing and keyboard
 * functionalities.
 *
 * @return A pointer to the internal buffer storing the line read, or `NULL`
 * on `EOF`.
 */
char *xd_readline();

/**
 * @brief Clears the history.
 */
void xd_readline_history_clear();

/**
 * @brief Adds a copy of the passed string (without trailing newline) to the
 * history.
 *
 * @param str The string to be added to the history, must be null-terminated.
 *
 * @return `0` on success or `-1` if the passed string is `NULL` or on
 * allcoation failure.
 */
int xd_readline_history_add(const char *str);

/**
 * @brief Retrieves a copy of the n-th entry from the history.
 *
 * If the passed integer is non-negative, the function returns the (n)-th entry
 * starting from the beginning of the history (i.e., 1 refers to the first
 * entry).
 *
 * If the passed integer is negative, the function returns the (-n)-th entry
 * starting from the end of the history (i.e., -1 refers to the last
 * entry).
 *
 * @param n The number of the history entry to be returned.
 *
 * @return A newly allocated string containing the requested history entry, or
 * `NULL` if the index is out of bounds or on memory allocation failure.
 */
char *xd_readline_history_get(int n);

/**
 * @brief Prints all history entries to the screen.
 */
void xd_readline_history_print();

/**
 * @brief Writes the history to a file.
 *
 * @param path The path of the file to write the history to.
 * @param append Whether to append to the file (non-zero) or overwrite it
 * (zero).
 *
 * @return `0` on success `-1` on failure.
 */
int xd_readline_history_save_to_file(const char *path, int append);

/**
 * @brief Loads the history from a file.
 *
 * @param path The path of the file to read the history from.
 *
 * @return `0` on success `-1` on failure.
 */
int xd_readline_history_load_from_file(const char *path);

#endif  // XD_READLINE_H
