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

#include <glob.h>
#include <limits.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "xd_readline.h"

/**
 * @brief Comparison function for sorting path strings.
 *
 * @param first Pointer to the first element being compared.
 * @param second Pointer to the second element being compared.
 *
 * @return A negative value if first should come before second, a positive
 * value if second should come before first, zero if both are equal.
 */
static int xd_path_cmp(const void *first, const void *second) {
  const char *path1 = *(const char **)first;
  const char *path2 = *(const char **)second;

  return strcasecmp(path1, path2);
}  // xd_path_cmp()

/**
 * @brief Generates path completions for the passed partial path.
 *
 * @param partial_path The partial path string to be complete.
 *
 * @return A newly-allocated, sorted, and null-terminated string array of
 * possible path completions.
 *
 * @note The caller is responsible for freeing the returned array and its
 * contents.
 */
char **xd_path_completions_generator(const char *partial_path) {
  // initialize glob pattern
  char pattern[PATH_MAX] = {0};
  snprintf(pattern, PATH_MAX, "%s*", partial_path);

  // find glob matches
  glob_t glob_result;
  if (glob(pattern, GLOB_TILDE_CHECK | GLOB_MARK | GLOB_NOSORT, NULL,
           &glob_result) != 0) {
    globfree(&glob_result);
    return NULL;
  }

  // allocate memory for the array of completions
  int completions_count = (int)glob_result.gl_pathc;
  char **completions =
      (char **)malloc(sizeof(char *) * (completions_count + 1));
  if (completions == NULL) {
    return NULL;
  }

  // copy the glob matches to the array of completions
  for (int i = 0; i < completions_count; i++) {
    completions[i] = strdup(glob_result.gl_pathv[i]);
    if (completions[i] == NULL) {
      // allocation failure, stop adding and return
      globfree(&glob_result);
      return completions;
    }
  }
  // add null-terminator to indicate end of array
  completions[completions_count] = NULL;
  globfree(&glob_result);

  // sort then return the array of completions
  qsort((void *)completions, completions_count, sizeof(char *), xd_path_cmp);
  return completions;
}  // xd_path_completions_generator()

/**
 * @brief The definition of `xd_readline_completions_generator`.
 */
char **xd_completions_generator(const char *line, int start, int end) {
  int partial_text_len = end - start;
  char partial_text[partial_text_len + 1];
  memcpy(partial_text, line + start, partial_text_len);
  partial_text[partial_text_len] = '\0';
  return xd_path_completions_generator(partial_text);
}  // xd_completions_generator()

/**
 * @brief Handles history expansion.
 *
 * @param line The line containing the history expansion.
 */
void xd_history_expansion(const char *line) {
  int num = atoi(line + 1);
  char *entry = xd_readline_history_get(num);
  if (entry != NULL) {
    printf("Expansion: %s\n---------------------\n", entry);
    free(entry);
  }
}  // history_expansion()

int main() {
  xd_readline_prompt = "\033[0;101mxd\033[0m-rl> ";
  xd_readline_completions_generator = xd_completions_generator;

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
