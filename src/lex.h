#pragma once

#include <util.h>
#include <enum-int.h>
enum LEX_TYPES
{
#include "lex-enums.txt"
};

typedef struct LEXICAL_UNIT
{
  uint32_t l_line_n;
  uint32_t l_column_n;
  uint8_t l_type;
  union
  {
    // LX_CHAR scanned.
    char l_char;
    // LX_IDENTIFIER or LX_.._KW scanned.
    char l_name[MAX_STR + 1];
    // LX_NUMBER scanned.
    int32_t l_number;
  };
} LEXICAL_UNIT;

typedef char (*INPUT_FUNCTION)(void *, bool);

void lex_set_input_function(INPUT_FUNCTION input_function, void *data);
bool lex_is_keyword(uint8_t lex_type);
bool lex_is_symbol(uint8_t lex_type);
void lex_print(LEXICAL_UNIT *p_lex);
bool lex_scan(void);
// Included for testing.
char lex_get_char(bool peek_char);
