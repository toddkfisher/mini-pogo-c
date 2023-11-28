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

// THEORY OF OPERATION:
//   Input is taken from char (*g_lex_input_function)(void *p_personal_data, bool peek_char), a global
//   function pointer variable. Set the function and pointer to its data with
//   lex_set_input_function(fn, void *data).  g_lex_input_function() should be ready to
//   read on the first call.  Initialization should take place prior to
//   lex_set_input_function().
//
//   -- USER-SUPPLIED lex_input_function() SHOULD:
//     1. return EOF when it exhausts it's input.
//     2. return and consume a character when passed a value of peek_char == false.
//     3. return and not consume a character when passed a value of peek_char == true.
//
//   -- SCANNING LEXICAL UNITS:
//     1. The name of the input source, typically a filename, is stored in the global variable:
//        g_input_name.
//     2. To scan the next lexical unit from the input source, call lex_scan().
//     3. The source line number and source col number are stored in the global variables:
//        g_input_line_n, g_input_column_n respectively.
//     4. The last lexical unit scanned is stored in the global variable: g_current_lex_unit
//        (type LEXICAL_UNIT).
//     5. Once end-of-file is reached, g_current_lex_unit.lex_type == LX_EOF thereafter.

#include <enum-str.h>
// Lexical type (LX_..) names as strings. Ex: lex_names[LX_IDENTIFIER] == "LX_IDENTIFIER".
char *g_lex_names[] =
{
#include "lex-enums.txt"
};

static INPUT_FUNCTION g_lex_input_function;
// Input data pointer given to g_lex_input_function() on each call.
static void *g_input_data;

LEXICAL_UNIT g_current_lex_unit;
uint32_t g_input_line_n = 1;
uint32_t g_input_column_n = 1;

bool g_lex_debug_print = false;   // Print lexical units as they are scanned.

// Given to us by God.  A gift from Heaven above.
char *ASCII[] =
{
  [0x00] = "NUL", [0x01] = "SOH", [0x02] = "STX", [0x03] = "ETX",
  [0x04] = "EOT", [0x05] = "ENQ", [0x06] = "ACK", [0x07] = "BEL",
  [0x08] = "BS",  [0x09] = "HT",  [0x0A] = "LF",  [0x0B] = "VT",
  [0x0C] = "FF",  [0x0D] = "CR",  [0x0E] = "SO",  [0x0F] = "SI",
  [0x10] = "DLE", [0x11] = "DC1", [0x12] = "DC2", [0x13] = "DC3",
  [0x14] = "DC4", [0x15] = "NAK", [0x16] = "SYN", [0x17] = "ETB",
  [0x18] = "CAN", [0x19] = "EM",  [0x1A] = "SUB", [0x1B] = "ESC",
  [0x1C] = "FS",  [0x1D] = "GS",  [0x1E] = "RS",  [0x1F] = "US",
  [0x20] = "SPC", [0x21] = "!",   [0x22] = "\"",  [0x23] = "#",
  [0x24] = "$",   [0x25] = "%",   [0x26] = "&",   [0x27] = "'",
  [0x28] = "(",   [0x29] = ")",   [0x2A] = "*",   [0x2B] = "+",
  [0x2C] = ",",   [0x2D] = "-",   [0x2E] = ".",   [0x2F] = "/",
  [0x30] = "0",   [0x31] = "1",   [0x32] = "2",   [0x33] = "3",
  [0x34] = "4",   [0x35] = "5",   [0x36] = "6",   [0x37] = "7",
  [0x38] = "8",   [0x39] = "9",   [0x3A] = ":",   [0x3B] = ";",
  [0x3C] = "<",   [0x3D] = "=",   [0x3E] = ">",   [0x3F] = "?",
  [0x40] = "@",   [0x41] = "A",   [0x42] = "B",   [0x43] = "C",
  [0x44] = "D",   [0x45] = "E",   [0x46] = "F",   [0x47] = "G",
  [0x48] = "H",   [0x49] = "I",   [0x4A] = "J",   [0x4B] = "K",
  [0x4C] = "L",   [0x4D] = "M",   [0x4E] = "N",   [0x4F] = "O",
  [0x50] = "P",   [0x51] = "Q",   [0x52] = "R",   [0x53] = "S",
  [0x54] = "T",   [0x55] = "U",   [0x56] = "V",   [0x57] = "W",
  [0x58] = "X",   [0x59] = "Y",   [0x5A] = "Z",   [0x5B] = "[",
  [0x5C] = "\\",  [0x5D] = "]",   [0x5E] = "^",   [0x5F] = "_",
  [0x60] = "`",   [0x61] = "a",   [0x62] = "b",   [0x63] = "c",
  [0x64] = "d",   [0x65] = "e",   [0x66] = "f",   [0x67] = "g",
  [0x68] = "h",   [0x69] = "i",   [0x6A] = "j",   [0x6B] = "k",
  [0x6C] = "l",   [0x6D] = "m",   [0x6E] = "n",   [0x6F] = "o",
  [0x70] = "p",   [0x71] = "q",   [0x72] = "r",   [0x73] = "s",
  [0x74] = "t",   [0x75] = "u",   [0x76] = "v",   [0x77] = "w",
  [0x78] = "x",   [0x79] = "y",   [0x7A] = "z",   [0x7B] = "{",
  [0x7C] = "|",   [0x7D] = "}",   [0x7E] = "~",   [0x7F] = "DEL"
};

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
  switch (p_lex->l_type)
  {
    case LX_IDENTIFIER:
      printf(" %s\n", p_lex->l_name);
      break;
    case LX_NUMBER:
      printf(" %d\n", p_lex->l_number);
      break;
    case LX_CHAR:
      printf(" %s\n", ASCII[(uint8_t) p_lex->l_char]);
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
    if ('\n' == ch)
    {
      g_input_line_n += 1;
      g_input_column_n = 1;
    }
    else
      g_input_column_n += 1;
  }
  return ch;
}

static bool lex_is_whitespace(char ch)
{
  return ch > '\0'&& ch <= ' ';
}

static void lex_skip_to(char ch, bool skip_over)
{
  while (ch != lex_get_char(true))
    lex_get_char(false);
  if (skip_over && EOF != lex_get_char(true))
    lex_get_char(false);
}

static void lex_skip_whitespace(void)
{
  char ch;
  while(lex_is_whitespace(lex_get_char(true)))
  {
    ch = lex_get_char(false);
    if ('!' == ch)
      lex_skip_to('\n', true);
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
  char ch = tolower(lex_get_char(true));
  i = 0;
  zero_mem(l_name, MAX_STR);
  do
  {
    if (i < MAX_STR - 1)
      l_name[i] = ch;
    i += 1;
    ch = tolower(lex_get_char(false));
  } while ('_' == ch || isalnum(ch));
  i = 0;
  while (keyword_to_type_table[i].kw_name[0] != '\0' &&
         (scmp = strcmp(keyword_to_type_table[i].kw_name, l_name)) < 0)
  {
    i += 1;
  }
  if ('\0' != keyword_to_type_table[i].kw_name[0] && 0 == scmp)
    g_current_lex_unit.l_type = keyword_to_type_table[i].kw_type;
  else
    g_current_lex_unit.l_type = LX_IDENTIFIER;
  return true;
}

static bool lex_scan_number(void)
{
  int32_t n = 0;
  while (isdigit(lex_get_char(true)))
  {
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
  switch (ch)
  {
    case ':':
      if ('=' == lex_get_char(true))
      {
        l_type = LX_ASSIGN_SYM;
        // Skip '='
        lex_get_char(false);
      }
      else
        retval = false;
      break;
    case '=':
      l_type = LX_EQ_SYM;
      break;
    case '>':
      if ('=' == lex_get_char(true))
      {
        l_type = LX_GE_SYM;
        // Skip '='
        lex_get_char(false);
      }
      else
        l_type = LX_GT_SYM;
      break;
    case '<':
      if ('=' == lex_get_char(true))
      {
        l_type = LX_LE_SYM;
        // Skip '='
        lex_get_char(false);
      }
      else
        l_type = LX_LT_SYM;
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

uint8_t lex_x_digit_value(char digit_char)
{
  uint8_t result;
  digit_char = tolower(digit_char);
  if (digit_char >= '0' && digit_char <= '9')
    result = digit_char - '0';
  else
    result = 10 + digit_char - 'a';
  return result;
}

bool lex_scan_char(void)
{
  char ch = lex_get_char(false);  // Skip over single quote
  bool result = false;
  switch (ch)
  {
    case '\\':  // '\x<hexdigit>+' or '\n' or '\t' or '\''
      ch = lex_get_char(false);  // Skip over backslash.
      if (EOF == ch)
        g_current_lex_unit.l_type = LX_ERROR;
      else
      {
        g_current_lex_unit.l_type = LX_CHAR;
        if ('x' != tolower(ch))
        {
          // '\n' or '\t' or '\<anychar but eof>'
          switch (ch)
          {
            case 'n':
              g_current_lex_unit.l_char = '\n';
              break;
            case 't':
              g_current_lex_unit.l_char = '\t';
              break;
            default:
              g_current_lex_unit.l_char = ch;
              break;
          }
          ch = lex_get_char(false);  // Skip over character following backslash.
          if ('\'' != ch)
            g_current_lex_unit.l_type = LX_ERROR;
          else
          {
            lex_get_char(false);  // Skip over closing single quote.
            result = true;
          }
        }
        else
        {
          // '\x<hexdigit>+' (wraps around 127).
          uint8_t n = 0;
          ch = lex_get_char(false);  // Skip 'x'
          while (isxdigit(ch))
          {
            n = ((n << 4) + lex_x_digit_value(ch));
            n &= 0x7f;  // ASCII only bitches.
            ch = lex_get_char(false);
          }
          if ('\'' != ch)
            g_current_lex_unit.l_type = LX_ERROR;
          else
          {
            g_current_lex_unit.l_type = LX_CHAR;
            g_current_lex_unit.l_char = n;
            result = true;
            lex_get_char(false);  // Skip over closing single quote.
          }
        }
      }
      break;
    default:  // '<char>'
      g_current_lex_unit.l_type = LX_CHAR;
      g_current_lex_unit.l_char = ch;
      ch = lex_get_char(false);
      if (EOF == ch)
        g_current_lex_unit.l_type = LX_ERROR;
      else
      {
        if ('\'' != ch)
          g_current_lex_unit.l_type = LX_ERROR;
        else
        {
          lex_get_char(false);  // Skip over single quote.
          result = true;
        }
      }
      break;
  }
  return result;
}

bool lex_scan(void)
{
  char ch;
  bool retval = true;
  lex_skip_whitespace();
  g_current_lex_unit.l_line_n = g_input_line_n;
  g_current_lex_unit.l_column_n = g_input_column_n - 1;
  ch = lex_get_char(true);
  if (EOF == ch)
    retval = g_current_lex_unit.l_type = LX_EOF;
  else if (isalpha(ch))
    retval = lex_scan_identifier_or_keyword();
  else if (isdigit(ch))
    retval = lex_scan_number();
  else if ('\'' == ch)
    retval = lex_scan_char();
  else if (ispunct(ch))
    retval = lex_scan_symbol();
  else
  {
    g_current_lex_unit.l_type = LX_ERROR;
    retval = false;
  }
  if (g_lex_debug_print)
    lex_print(&g_current_lex_unit);
  return retval;
}
