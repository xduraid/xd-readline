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
#include <string.h>

#include "xd_readline.h"

int main() {
  xd_readline_prompt = "xd-rl> ";
  char *line = NULL;
  while ((line = xd_readline()) != NULL) {
    printf("Read: %s---------------------\n", line);
    xd_readline_history_add(line);
    if (strcmp(line, "history\n") == 0) {
      xd_readline_history_print();
    }
    else if (strcmp(line, "history -c\n") == 0) {
      xd_readline_history_clear();
    }
    else if (strcmp(line, "exit\n") == 0) {
      break;
    }
  }
  exit(EXIT_SUCCESS);
}  // main()
