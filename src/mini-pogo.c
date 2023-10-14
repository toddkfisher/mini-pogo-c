#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <cmdline-switch.h>

#include "lex.h"
#include "mp-prototypes.h"

extern LEXICAL_UNIT g_current_lex_unit;
extern uint32_t g_input_line_n;
extern uint32_t g_input_column_n;
extern char *g_lex_names[];

// Sequence of expected lexical types from test-module-3.pogo.
uint8_t test_module_3_pogo_lex_types[] = {
  LX_MODULE_KW,            // module
  LX_IDENTIFIER,           // test
  LX_INIT_KW,              // init
  LX_IF_KW,                // if
  LX_IDENTIFIER,           // a
  LX_GT_SYM,               // >
  LX_NUMBER,               // 1
  LX_OR_KW,                // or
  LX_IDENTIFIER,           // b
  LX_EQ_SYM,               // =
  LX_NUMBER,               // 3
  LX_AND_KW,               // and
  LX_IDENTIFIER,           // c
  LX_LT_SYM,               // <
  LX_IDENTIFIER,           // b
  LX_THEN_KW,              // then
  LX_PRINT_INT_KW,         // print_int
  LX_NUMBER,               // 123
  LX_SEMICOLON_SYM,        // ;
  LX_ELSE_KW,              // else
  LX_PRINT_INT_KW,         // print_int
  LX_NUMBER,               // 666
  LX_SEMICOLON_SYM,        // ;
  LX_END_KW,               // end
  LX_SEMICOLON_SYM,        // ;
  LX_IDENTIFIER,           // x
  LX_ASSIGN_SYM,           // :=
  LX_NUMBER,               // 1
  LX_PLUS_SYM,             // +
  LX_IDENTIFIER,           // x
  LX_SEMICOLON_SYM,        // ;
  LX_IDENTIFIER,           // y
  LX_ASSIGN_SYM,           // :=
  LX_NUMBER,               // 6
  LX_DIVIDE_SYM,           // /
  LX_NUMBER,               // 2
  LX_TIMES_SYM,            // *
  LX_LPAREN_SYM,           // (
  LX_NUMBER,               // 2
  LX_PLUS_SYM,             // +
  LX_NUMBER,               // 1
  LX_RPAREN_SYM,           // )
  LX_SEMICOLON_SYM,        // ;
  LX_SPAWN_KW,             // spawn
  LX_IDENTIFIER,           // t0
  LX_SEMICOLON_SYM,        // ;
  LX_IDENTIFIER,           // t1
  LX_SEMICOLON_SYM,        // ;
  LX_IDENTIFIER,           // t1
  LX_SEMICOLON_SYM,        // ;
  LX_END_KW,               // end
  LX_SEMICOLON_SYM,        // ;
  LX_END_KW,               // end
  LX_SEMICOLON_SYM,        // ;
  LX_TASK_KW,              // task
  LX_IDENTIFIER,           // t0
  LX_SEMICOLON_SYM,        // ;
  LX_STOP_KW,              // stop
  LX_SEMICOLON_SYM,        // ;
  LX_END_KW,               // end
  LX_SEMICOLON_SYM,        // ;
  LX_TASK_KW,              // task
  LX_IDENTIFIER,           // t1
  LX_SEMICOLON_SYM,        // ;
  LX_IF_KW,                // if
  LX_NUMBER,               // 0
  LX_THEN_KW,              // then
  LX_STOP_KW,              // stop
  LX_SEMICOLON_SYM,        // ;
  LX_STOP_KW,              // stop
  LX_SEMICOLON_SYM,        // ;
  LX_END_KW,               // end
  LX_SEMICOLON_SYM,        // ;
  LX_END_KW,               // end
  LX_SEMICOLON_SYM,        // ;
  LX_END_KW,               // end
  LX_SEMICOLON_SYM,        // ;
  LX_EOF
};

// Data block for file_input().
typedef struct {
  FILE *f_file;
  char f_current_char;
  bool f_first_read_occured;
} FILE_READ;

char file_input(void *p_data, bool peek_char) {
  FILE_READ *p_fr = (FILE_READ *) p_data;
  char retval;
  if (peek_char && p_fr->f_first_read_occured) {
    retval = p_fr->f_current_char;
  } else {
    retval = p_fr->f_current_char = fgetc(p_fr->f_file);
    p_fr->f_first_read_occured = true;
  }
  return retval;
}

void test_lex(char *filename)
{
  FILE_READ fr;
  uint32_t lex_unit_n = 0;
  if (NULL == (fr.f_file = fopen(filename, "r"))) {
    fprintf(stderr, "%s : not found\n", filename);
  } else {
    fr.f_first_read_occured = false;
    lex_set_input_function(file_input, &fr);
    do {
      lex_scan();
      //lex_print(&g_current_lex_unit);
      if (g_current_lex_unit.lu_type != test_module_3_pogo_lex_types[lex_unit_n]) {
        printf("%d:%d. Incorrect lex unit scanned.  Got: %s, expected %s\n",
               g_current_lex_unit.lu_line_n, g_current_lex_unit.lu_column_n,
               g_lex_names[g_current_lex_unit.lu_type],
               g_lex_names[test_module_3_pogo_lex_types[lex_unit_n]]);
        exit(0);
      }
      lex_unit_n += 1;
    } while (LX_EOF != g_current_lex_unit.lu_type);
    printf("All lexical units match what was expected.\n");
  }
}

void test_file_read(char *filename)
{
  FILE_READ fr;
  char ch;
  if (NULL == (fr.f_file = fopen(filename, "r"))) {
    fprintf(stderr, "%s : not found\n", filename);
  } else {
    fr.f_first_read_occured = false;
    lex_set_input_function(file_input, &fr);
    while (EOF != (ch = lex_get_char(false))) {
      putchar(ch);
    }
  }
}

char *test_module_3_pogo_text =
  "module test\n"
  "  init\n"
  "    if a>1 or b=3 and c<b then\n"
  "      print_int 123;\n"
  "    else\n"
  "      print_int 666;\n"
  "    end;\n"
  "    x := 1 + x;\n"
  "    y := 6/2*(2+1);\n"
  "    spawn t0; t1; t1; end;\n"
  "  end;\n"
  " \n"
  "  task t0;\n"
  "    stop;\n"
  "  end;\n"
  " \n"
  "  task t1;\n"
  "    if 0 then\n"
  "      stop;\n"
  "      stop;\n"
  "    end;\n"
  "  end;\n"
  "end;\n";

// Data block for in-memory input.
typedef struct MEM_READ {
  char *m_entire_text;
  char *m_p_current;
} MEM_READ;

char mem_input(void *p_data, bool peek_char)
{
  MEM_READ *p_mem_read = (MEM_READ *) p_data;
  char ch = *p_mem_read->m_p_current;
  char result = ch ? ch : EOF;
  if (ch && !peek_char) {
    p_mem_read->m_p_current += 1;
  }
  return result;
}

void test_mem_read(void)
{
  MEM_READ mread;
  char ch;
  mread.m_entire_text = mread.m_p_current = test_module_3_pogo_text;
  lex_set_input_function(mem_input, &mread);
  while (EOF != (ch = lex_get_char(false))) {
    putchar(ch);
  }
}

enum { S_TEST_FILE_READ, S_TEST_MEM_READ, S_TEST_LEX };

SWITCH g_lex_test_switches[] = {
//  s_switch_id       s_long_name         s_short_name     s_max_parameters    s_allow_dashed_parmameters
  { S_TEST_FILE_READ, "--file-read-test", "-f",            1,                  false },
  { S_TEST_MEM_READ,  "--mem-read-test",  "-m",            0,                  false },
  { S_TEST_LEX,       "--lex-test",       "-l",            1,                  false },
  SWITCH_LIST_END
};

int main(int argc, char **argv)
{
  int32_t n_params = 0;
  uint32_t switch_id;
  int argv_idx = 1;
  char *switch_params[255];
  if (argc <= 1) {
    fprintf(stderr, "usage: mp [-f <file> | -m | -l <file>]\n");
  } else {
    while (n_params >= 0 && argv_idx < argc) {
      n_params = cs_parse(argc, argv, g_lex_test_switches, &switch_id, &argv_idx, switch_params);
      if (n_params < 0) {
        fprintf(stderr, "Unknown switch: %s\n", argv[argv_idx]);
      } else {
        switch (switch_id) {
          case S_TEST_FILE_READ:
            if (n_params != 1) {
              fprintf(stderr, "Usage: mp -f <filename>\n");
            } else {
              test_file_read(switch_params[0]);
            }
            break;
          case S_TEST_MEM_READ:
            test_mem_read();
            break;
          case S_TEST_LEX:
            if (n_params != 1) {
              fprintf(stderr, "Usage: mp -l <filename>\n");
            } else {
              test_lex(switch_params[0]);
            }
            break;
          default:
            break;
        }
      }
    }
  }
  return 0;
}
