/*
 * Author: Konrad Słoniewski
 * Date:   18 August 2013
 */

#include <string.h>
#include <stdlib.h>
#include "help_functions.h"

int count_occurrences(const char* string, char c) {
  int count, i;
  count = 0;
  i = 0;
  for (i = 0; i < strlen(string); i += 1)
    if (string[i] == c)
      count += 1;
  return count;
}

char** split(const char* raw, const char* separator, int limit) {
  char *string;
  char *token;
  char **out;
  int i; 
 
  out = NULL;
  string = strdup(raw);
  i = 0;
  
  if (string != NULL) {
    char *head = string;
    out = malloc(limit * sizeof(char *));
    
    while (i < limit && (token = strsep(&string, separator)) != NULL) {
      out[i] = strdup(token);
      i += 1;
    }
    free(head);
  }

  return out;
}  
