#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <util.h>
//------------------------------------------------------------------------------
#include "string-table.h"
//------------------------------------------------------------------------------
STRING_CONST *g_hash_strings[STRING_HTABLE_SIZE];
uint32_t g_n_strings = 0;
//------------------------------------------------------------------------------
void strtab_init(void)
{
  g_n_strings = 0;
  for (int i = 0; i < STRING_HTABLE_SIZE; ++i)
    g_hash_strings[i] = NULL;
}
//------------------------------------------------------------------------------
static uint32_t strtab_hash(char *s)
{
  uint32_t char_sum = 0;
  uint32_t result;
  while (*s)
    char_sum += *s++;
  result = char_sum % STRING_HTABLE_SIZE;
  return result;
}
//------------------------------------------------------------------------------
STRING_CONST *strtab_lookup_string(char *s)
{
  STRING_CONST *result = NULL;
  for (result = g_hash_strings[strtab_hash(s)];
       result && !STREQ(s, result->s_string);
       result = result->s_p_next)
  {
    // Empty loop body.  Everything happens in for loop header.
  }
  return result;
}
//------------------------------------------------------------------------------
STRING_CONST *strtab_add_string(char *s)
{
  STRING_CONST *result = strtab_lookup_string(s);
  if (!result)
  {
    uint32_t h = strtab_hash(s);
    result = malloc(sizeof(STRING_CONST));
    strcpy(result->s_string, s);
    result->s_table_index = g_n_strings++;
    result->s_p_next = g_hash_strings[h];
    g_hash_strings[h] = result;
  }
  return result;
}
