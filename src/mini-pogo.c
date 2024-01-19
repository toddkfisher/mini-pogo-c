#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <cmdline-switch.h>

#include "lex.h"
#include "parse.h"
#include "compile.h"
#include "binary-header.h"

#define macstr(x) #x

enum COMPILE_FLAGS
{
  CF_NONE = 0,
  CF_HEADER = 1,
  CF_CODE = 2
};

extern LEXICAL_UNIT g_current_lex_unit;
extern uint32_t g_input_line_n;
extern uint32_t g_input_column_n;
extern char *g_lex_names[];
extern bool g_lex_debug_print;  // Print lexical units as they are scanned.

// Sequence of expected lexical types from test-module-3.pogo.
uint8_t test_module_3_pogo_lex_types[] =
{
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
typedef struct
{
  FILE *f_file;
  char f_current_char;
  bool f_first_read_occured;
} FILE_READ;

char file_input(void *const p_data, bool const peek_char)
{
  FILE_READ *p_fr = (FILE_READ *) p_data;
  char retval;
  if (peek_char && p_fr->f_first_read_occured)
    retval = p_fr->f_current_char;
  else
  {
    retval = p_fr->f_current_char = fgetc(p_fr->f_file);
    p_fr->f_first_read_occured = true;
  }
  return retval;
}

void test_lex(char *const filename)
{
  FILE_READ fr;
  uint32_t lex_unit_n = 0;
  if (NULL == (fr.f_file = fopen(filename, "r")))
    fprintf(stderr, "%s : not found\n", filename);
  else
  {
    fr.f_first_read_occured = false;
    lex_set_input_function(file_input, &fr);
    do
    {
      lex_scan();
      if (g_current_lex_unit.l_type != test_module_3_pogo_lex_types[lex_unit_n])
      {
        printf("%d:%d. Incorrect lex unit scanned.  Got: %s, expected %s\n",
               g_current_lex_unit.l_line_n, g_current_lex_unit.l_column_n,
               g_lex_names[g_current_lex_unit.l_type],
               g_lex_names[test_module_3_pogo_lex_types[lex_unit_n]]);
        fclose(fr.f_file);
        error_exit(0);
      }
      lex_unit_n += 1;
    } while (LX_EOF != g_current_lex_unit.l_type);
    printf("All lexical units match what was expected.\n");
    fclose(fr.f_file);
  }
}

void scan_file_and_print(char *const filename)
{
  FILE_READ fr;
  if (NULL == (fr.f_file = fopen(filename, "r")))
    fprintf(stderr, "%s : not found\n", filename);
  else
  {
    fr.f_first_read_occured = false;
    lex_set_input_function(file_input, &fr);
    do
    {
      lex_scan();
      lex_print(&g_current_lex_unit);
    } while (LX_EOF != g_current_lex_unit.l_type && LX_ERROR != g_current_lex_unit.l_type);
  }
}

void test_parse(char *const filename)
{
  FILE_READ fr;
  if (NULL == (fr.f_file = fopen(filename, "r")))
    fprintf(stderr, "%s : not found\n", filename);
  else
  {
    fr.f_first_read_occured = false;
    lex_set_input_function(file_input, &fr);
    PARSE_NODE *p_node;
    if (NULL != (p_node = parse()))
      parse_print_tree(1, p_node);
    fclose(fr.f_file);
  }
}

void compile_selectively(uint32_t compile_flags,
                         char *input_filename,
                         char *output_filename)
{
  FILE_READ fr;
  FILE *fout;
  if (NULL == (fr.f_file = fopen(input_filename, "r")))
    fprintf(stderr, "%s : not found\n", input_filename);
  else
  {
    PARSE_NODE *p_tree;
    fr.f_first_read_occured = false;
    lex_set_input_function(file_input, &fr);
    if (NULL != (p_tree = parse()))
    {
      if (NULL == (fout = fopen(output_filename, "w")))
      {
        fprintf(stderr, "%s : cannot open\n", output_filename);
        fclose(fr.f_file);
      }
      else
      {
        compile_init();
        compile(p_tree);
        if (compile_flags & CF_HEADER)
          compile_write_header(fout);
        if (compile_flags & CF_CODE)
          compile_write_code(fout);
        fclose(fout);
        fclose(fr.f_file);
      }
    }
  }
}

void test_file_read(char *filename)
{
  FILE_READ fr;
  char ch;
  if (NULL == (fr.f_file = fopen(filename, "r")))
    fprintf(stderr, "%s : not found\n", filename);
  else
  {
    fr.f_first_read_occured = false;
    lex_set_input_function(file_input, &fr);
    while (EOF != (ch = lex_get_char(false)))
      putchar(ch);
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
typedef struct MEM_READ
{
  char *m_entire_text;
  char *m_p_current;
} MEM_READ;

char mem_input(void *p_data, bool peek_char)
{
  MEM_READ *p_mem_read = (MEM_READ *) p_data;
  char ch = *p_mem_read->m_p_current;
  char result = ch ? ch : EOF;
  if (ch && !peek_char)
    p_mem_read->m_p_current += 1;
  return result;
}

void test_mem_read(void)
{
  MEM_READ mread;
  char ch;
  mread.m_entire_text = mread.m_p_current = test_module_3_pogo_text;
  lex_set_input_function(mem_input, &mread);
  while (EOF != (ch = lex_get_char(false)))
    putchar(ch);
}

enum
{
  S_HELP,
  S_TEST_FILE_READ,
  S_TEST_MEM_READ,
  S_TEST_LEX,
  S_LEX_PRINT,
  S_TEST_PARSE,
  S_COMPILE_HEADER,
  S_COMPILE
};

SWITCH g_lex_test_switches[] =
{
  //  s_switch_id      s_long_name                s_short_name  s_min_parameters s_max_parameters    s_usage                                             s_flags
  { S_HELP,             "--help",                 "-h",         0,               0,                  "usage: --help",                                           CS_PARAM_ERROR_ALL },
  { S_TEST_FILE_READ,   "--file-read-test",       "-f",         1,               1,                  "usage: --file-read-test <input file>",                    CS_PARAM_ERROR_ALL },
  { S_TEST_MEM_READ,    "--mem-read-test",        "-m",         0,               0,                  "usage: --mem-read-test",                                  CS_PARAM_ERROR_ALL },
  { S_TEST_LEX,         "--lex-test",             "-l",         1,               1,                  "usage: --lex-test <input file>",                          CS_PARAM_ERROR_ALL },
  { S_LEX_PRINT,        "--lex-print",            "",           1,               1,                  "usage: --lex-print",                                      CS_PARAM_ERROR_ALL },
  { S_TEST_PARSE,       "--parse-test",           "-p",         1,               1,                  "usage: --parse-test <input file>",                        CS_PARAM_ERROR_ALL },
  { S_COMPILE_HEADER,   "--compile-header-only",  "",           2,               2,                  "usage: --compile-header-only <input file> <output file>", CS_PARAM_ERROR_ALL },
  { S_COMPILE,          "--compile",              "-c",         2,               2,                  "usage: --compile <input file> <output file>",             CS_PARAM_ERROR_ALL },
  SWITCH_LIST_END
};

void help(void)
{
  fprintf(stderr, "OPTIONS:\n");
  fprintf(stderr, "--help | -h                                       This help message.\n");
  fprintf(stderr, "(--file-read-test | -f) <input file>              Test generic read on file.\n");
  fprintf(stderr, "(--mem-read-test | -m)                            Test generic read on constant string.\n");
  fprintf(stderr, "(--lex-test | -l) <input file>                    Compare file lexical units against predefined array.\n");
  fprintf(stderr, "--lex-print file  <input file>                    Print internal representation of each lexical unit scanned from file.\n");
  fprintf(stderr, "(--parse-test | -p) <input file>                  Parse file.  Write parse tree outline to stdout (in org format).\n");
  fprintf(stderr, "--compile-header-only  <input file> <output file> Write header and no code to output-file.\n");
  fprintf(stderr, "--compile <input file> <output file>              Write header code to output-file.\n");
}

int main(int argc, char **argv)
{
  int32_t n_params = 0;
  uint32_t switch_id;
  int argv_idx = 1;
  char *switch_params[255];
  if (argc <= 1)
    help();
  else
  {
    while (n_params >= 0 && argv_idx < argc)
    {
      n_params = cs_parse(argc, argv,
                          g_lex_test_switches,
                          &switch_id,
                          &argv_idx,
                          switch_params);
      if (n_params < 0)
        fprintf(stderr, "Unknown switch: %s\n", argv[argv_idx]);
      else
      {
        switch (switch_id)
        {
          case S_TEST_FILE_READ:
            test_file_read(switch_params[0]);
            break;
          case S_TEST_MEM_READ:
            test_mem_read();
            break;
          case S_TEST_LEX:
            test_lex(switch_params[0]);
            break;
          case S_TEST_PARSE:
            test_parse(switch_params[0]);
            break;
          case S_LEX_PRINT:
            g_lex_debug_print = true;
            break;
          case S_COMPILE_HEADER:
            compile_selectively(CF_HEADER, switch_params[0], switch_params[1]);
            break;
          case S_COMPILE:
            compile_selectively(CF_HEADER | CF_CODE , switch_params[0], switch_params[1]);
            break;
          case S_HELP:
            help();
            break;
          default:
            break;
        }
      }
    }
  }
  return 0;
}
