/*
 * ==============================================================================
 * File: main.c
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

#include <stdio.h>
#include <stdlib.h>

#include "xd_readline.h"

int main() {
  if (xd_readline() == NULL) {
    puts("Hello World");
  }
  exit(EXIT_SUCCESS);
}  // main()
