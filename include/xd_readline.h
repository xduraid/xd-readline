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
 * @brief Reads a line from standard input with custom editing and keyboard
 * functionalities.
 *
 * @return A pointer to the internal buffer storing the line read, or `NULL`
 * on `EOF`.
 */
char *xd_readline();

#endif  // XD_READLINE_H
