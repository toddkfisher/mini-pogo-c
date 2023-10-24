#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <getopt.h>
#include <unistd.h>
#include <util.h>
#include <cmdline-switch.h>

#include "lex.h"

/*
 THEORY OF OPERATION:
   Input is taken from char (*g_lex_input_function)(void *p_personal_data, bool peek_char), a global
   function pointer variable. Set the function and pointer to its data with
   lex_set_input_function(fn, void *data).  g_lex_input_function() should be ready to
   read on the first call.  Initialization should take place prior to
   lex_set_input_function().

   -- USER-SUPPLIED lex_input_function() SHOULD:
     1. return EOF when it exhausts it's input.
     2. return and consume a character when passed a value of peek_char == false.
     3. return and not consume a character when passed a value of peek_char == true.

   -- SCANNING LEXICAL UNITS:
     1. The name of the input source, typically a filename, is stored in the global variable:
        g_input_name.
     2. To scan the next lexical unit from the input source, call lex_scan().
     3. The source line number and source col number are stored in the global variables:
        g_input_line_n, g_input_column_n respectively.
     4. The last lexical unit scanned is stored in the global variable: g_current_lex_unit
        (type LEXICAL_UNIT).
     5. Once end-of-file is reached, g_current_lex_unit.lex_type == LX_EOF thereafter.
*/

#include <enum-str.h>
// Lexical type (LX_..) names as strings. Ex: lex_names[LX_IDENTIFIER] == "LX_IDENTIFIER".
char *g_lex_names[] = {
#include "lex-enums.txt"
};

static INPUT_FUNCTION g_lex_input_function;
// Input data pointer given to g_lex_input_function() on each call.
static void *g_input_data;

LEXICAL_UNIT g_current_lex_unit;
uint32_t g_input_line_n = 1;
uint32_t g_input_column_n = 1;

bool g_lex_debug_print = false;   // Print lexical units as they are scanned.

bool lex_is_keyword(uint8_t lex_type)
{
  return lex_type > LX_KEYWORD_BEGIN && lex_type < LX_KEYWORD_END;
}

bool lex_is_symbol(uint8_t lex_type)
{
  return lex_type > LX_SYMBOL_BEGIN && lex_type < LX_SYMBOL_END;
}

void lex_print(LEXICAL_UNIT *p_lex)
{
  printf("%s", g_lex_names[p_lex->l_type]);
  switch (p_lex->l_type) {
    case LX_IDENTIFIER:
      printf(" %s\n", p_lex->l_name);
      break;
    case LX_NUMBER:
      printf(" %d\n", p_lex->l_number);
      break;
    default:
      printf("\n");
      break;
  }
}

void lex_set_input_function(INPUT_FUNCTION input_function, void *data_pointer)
{
  g_lex_input_function = input_function;
  g_input_data = data_pointer;
}

// This function is a wrapper around g_lex_input_function() so that our code isn't
// littered with checks for newline and setting column/line numbers.  It's in one place here.
char lex_get_char(bool peek_char)
{
  char ch = (*g_lex_input_function)(g_input_data, peek_char);
  if (!peek_char)
  {
    if ('\n' == ch) {
      g_input_line_n += 1;
      g_input_column_n = 1;
    } else {
      g_input_column_n += 1;
    }
  }
  return ch;
}

static bool lex_is_whitespace(char ch)
{
  return ch > '\0'&& ch <= ' ';
}

static void lex_skip_whitespace(void)
{
  while(lex_is_whitespace(lex_get_char(true)))
  {
    lex_get_char(false);
  }
}

// Keyword spelling -> keyword type.
static struct
{
  char *kw_name;
  uint8_t kw_type;
} keyword_to_type_table[] =
{
  { "and",         LX_AND_KW        },
  { "do",          LX_DO_KW         },
  { "else",        LX_ELSE_KW       },
  { "end",         LX_END_KW        },
  { "if",          LX_IF_KW         },
  { "init",        LX_INIT_KW       },
  { "module",      LX_MODULE_KW     },
  { "not",         LX_NOT_KW        },
  { "or",          LX_OR_KW         },
  { "print_char",  LX_PRINT_CHAR_KW },
  { "print_int",   LX_PRINT_INT_KW  },
  { "spawn",       LX_SPAWN_KW      },
  { "stop",        LX_STOP_KW       },
  { "task",        LX_TASK_KW       },
  { "then",        LX_THEN_KW       },
  { "while",       LX_WHILE_KW      },
  { "",            0                },
};

static bool lex_scan_identifier_or_keyword(void)
{
  // Index into l_name or keyword_to_type_table.
  uint32_t i;
  // String compare result when searching for keyword.
  int scmp;
  char *l_name = &g_current_lex_unit.l_name[0];
  char ch = lex_get_char(true);
  i = 0;
  zero_mem(l_name, MAX_STR);
  do {
    if (i < MAX_STR - 1) {
      l_name[i] = ch;
    }
    i += 1;
    ch = lex_get_char(false);
  } while ('_' == ch || isalnum(ch));
  i = 0;
  while (keyword_to_type_table[i].kw_name[0] != '\0' &&
         (scmp = strcmp(keyword_to_type_table[i].kw_name, l_name)) < 0) {
    i += 1;
  }
  if ('\0' != keyword_to_type_table[i].kw_name[0] && 0 == scmp) {
    g_current_lex_unit.l_type = keyword_to_type_table[i].kw_type;
  } else {
    g_current_lex_unit.l_type = LX_IDENTIFIER;
  }
  return true;
}

static bool lex_scan_number(void)
{
  int32_t n = 0;
  while (isdigit(lex_get_char(true))) {
    n = n*10 + lex_get_char(true) - '0';
    // Skip over digit
    lex_get_char(false);
  }
  g_current_lex_unit.l_type = LX_NUMBER;
  g_current_lex_unit.l_number = n;
  return true;
}

static bool lex_scan_symbol(void)
{
  bool retval = true;
  uint8_t l_type = LX_ERROR;
  char ch = lex_get_char(true);
  lex_get_char(false);
  switch (ch) {
    case ':':
      if ('=' == lex_get_char(true)) {
        l_type = LX_ASSIGN_SYM;
        // Skip '='
        lex_get_char(false);
      } else {
        retval = false;
      }
      break;
    case '=':
      l_type = LX_EQ_SYM;
      break;
    case '>':
      if ('=' == lex_get_char(true)) {
        l_type = LX_GE_SYM;
        // Skip '='
        lex_get_char(false);
      } else {
        l_type = LX_GT_SYM;
      }
      break;
    case '<':
      if ('=' == lex_get_char(true)) {
        l_type = LX_LE_SYM;
        // Skip '='
        lex_get_char(false);
      } else {
        l_type = LX_LT_SYM;
      }
      break;
    case '(':
      l_type = LX_LPAREN_SYM;
      break;
    case ')':
      l_type = LX_RPAREN_SYM;
      break;
    case ';':
      l_type = LX_SEMICOLON_SYM;
      break;
    case '*':
      l_type = LX_TIMES_SYM;
      break;
    case '/':
      l_type = LX_DIVIDE_SYM;
      break;
    case '+':
      l_type = LX_PLUS_SYM;
      break;
    case '-':
      l_type = LX_MINUS_SYM;
      break;
    default:
      l_type = LX_ERROR;
      retval = false;
      break;
  }
  g_current_lex_unit.l_type = l_type;
  return retval;
}

bool lex_scan(void) {
  char ch;
  bool retval = true;
  lex_skip_whitespace();
  g_current_lex_unit.l_line_n = g_input_line_n;
  g_current_lex_unit.l_column_n = g_input_column_n - 1   ;
  ch = lex_get_char(true);
  if (EOF == ch) {
    retval = g_current_lex_unit.l_type = LX_EOF;
  } else if (isalpha(ch)) {
    retval = lex_scan_identifier_or_keyword();
  } else if (isdigit(ch)) {
    retval = lex_scan_number();
  } else if (ispunct(ch)) {
    retval = lex_scan_symbol();
  } else {
    g_current_lex_unit.l_type = LX_ERROR;
    retval = false;
  }
  if (g_lex_debug_print) {
    lex_print(&g_current_lex_unit);
  }
  return retval;
}
