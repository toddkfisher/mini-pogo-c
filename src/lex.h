#pragma once

#include <util.h>
#include <enum-int.h>
enum LEX_TYPES {
#include "lex-enums.txt"
};

typedef struct LEXICAL_UNIT {
  uint32_t lu_line_n;
  uint32_t lu_column_n;
  uint8_t lu_type;
  union {
    // LX_CHAR scanned.
    char lu_char;
    // LX_IDENTIFIER or LX_.._KW scanned.
    char lu_name[MAX_STR + 1];
    // LX_NUMBER scanned.
    int32_t lu_number;
  };
} LEXICAL_UNIT;

typedef char (*INPUT_FUNCTION)(void *, bool);
