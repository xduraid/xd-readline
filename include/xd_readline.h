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
#define XD_HISTORY_MAX (4)

/**
 * @brief Prompt string displayed at the beginning of each input line.
 *
 * Used to change the prompt displayed by `xd_readline()` before reading input.
 * If not set or `NULL`, no prompt will be displayed.
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

#endif  // XD_READLINE_H
