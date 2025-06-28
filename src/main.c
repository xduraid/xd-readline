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

void xd_history_expansion(const char *line) {
  int num = atoi(line + 1);
  char *entry = xd_readline_history_get(num);
  if (entry != NULL) {
    printf("Expansion: %s\n---------------------\n", entry);
    free(entry);
  }
}  // history_expansion()

int main() {
  xd_readline_prompt = "xd-rl> ";
  char *line = NULL;
  while ((line = xd_readline()) != NULL) {
    printf("Read: %s---------------------\n", line);
    if (strcmp(line, "history\n") == 0) {
      xd_readline_history_print();
    }
    else if (strcmp(line, "history -c\n") == 0) {
      xd_readline_history_clear();
      continue;
    }
    else if (strcmp(line, "history -r\n") == 0) {
      xd_readline_history_load_from_file("xd.history");
    }
    else if (strcmp(line, "history -w\n") == 0) {
      xd_readline_history_save_to_file("xd.history", 0);
    }
    else if (strcmp(line, "history -a\n") == 0) {
      xd_readline_history_save_to_file("xd.history", 1);
    }
    else if (line[0] == '!') {
      xd_history_expansion(line);
      continue;
    }
    else if (strcmp(line, "exit\n") == 0) {
      break;
    }
    xd_readline_history_add(line);
  }
  exit(EXIT_SUCCESS);
}  // main()
