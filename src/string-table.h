#pragma once


typedef struct STRING_CONST STRING_CONST;
struct STRING_CONST
{
  STRING_CONST *s_p_next;
  char s_string[MAX_STR];
  uint32_t s_table_index;  // Index into header list of string constants.
};


#define STRING_HTABLE_SIZE 4999
