#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <util.h>

#include "parse.h"
#include "lex.h"

// THEORY OF OPERATION:
// This C module creates a parse tree for the grammar below. PARSE_NODE_TYPES are given
// in upper-case.  Recursive-descent is the parsing method used and (almost) each
// left side nonterminal corresponds to one parsing function which returns a PARSE_NODE*.
//
// ND_MODULE:
//       module-declaration  = 'module' name
//                             'init' statement-sequence 'end' ';'
//                             task-declaration* 'end'
// ND_TASK_DECLARATION:
//          task-declaration = 'task' name statement-sequence 'end'
//
// ND_STATEMENT_SEQUENCE:
//        statement-sequence = (statement ';')*
//                 statement = assignment-statement
//                           | if-statement
//                           | while-statement
//                           | stop-statement
//                           | spawn-statement
//                           | print-int-statement
//                           | print-char-statement
//
// ND_ASSIGN:
//     assignment-statement = variable-name ':=' expression
//
// ND_IF:
//             if-statement = 'if' expression 'then' statement-sequence
//                             ['else' statement-sequence] 'end'
//
// ND_WHILE:
//          while-statement = 'while' expression 'do' statement-sequence 'end'
//
// ND_STOP:
//           stop-statement = 'stop'
//
// ND_SPAWN:
//          spawn-statement = 'spawn' (name ';')+ 'end'
//
// ND_PRINT_INT:
//      print-int-statement = 'print_int' expression
//
// ND_PRINT_CHAR:
//   print-char-statement = 'print_char' character-constant
//
// ND_OR:
//            or-expression = and-expression ('or' and-expression)*
//
// ND_AND:                     ND_NOT (subtree if 'not' occurs otherwise just comparison-expression)
//                            |---------------------------|
//           and-expression = ['not'] comparison-expression ('and' comparison-expression)*
//
// ND_(LE|LT|GE|GT|EQ|NE):
//    comparison-expression = expression [relation-operator expression]
//        relation-operator = '<' | '<=' | '>' | '>=' | '=' | '!='
//
// ND_(ADD|SUBTRACT):         ND_NEGATE: (subtree if '+' or '-' occurs at far left otherwise just term)
//                            |-------------|
//               expression = ['+'|'-'] term (addition-operator term)*
//
// ND_(MULTIPLY|DIVIDE):
//                     term = factor (multiplicative-operator factor)*
//
//                     factor =
// ND_IDENTIFIER:
//                          variable-name
// ND_NUMBER:
//                          | number
//
//                          | '(' expression ')'

extern LEXICAL_UNIT g_current_lex_unit;
extern uint32_t g_input_line_n;
extern uint32_t g_input_column_n;
extern char *g_lex_names[];

#include <enum-str.h>
char *g_parse_node_names[] = {
#include "parse-node-enum.txt"
};

#define N_SPACES_INDENT 1

void parse_print_tree(uint32_t indent_level, PARSE_NODE *const p_tree)
{
  for (uint32_t i = 0; i < indent_level; ++i) {
    for (uint32_t j = 0; j < N_SPACES_INDENT; ++j) {
      printf("*");
    }
  }
  if (NULL != p_tree) {
    printf(" %s: ", g_parse_node_names[p_tree->nd_type]);
    switch (p_tree->nd_type) {
      case ND_NUMBER:
        printf("%d\n", p_tree->nd_number);
        break;
      case ND_VARIABLE:
        printf("%c\n", p_tree->nd_var_name);
        break;
      case ND_MULTIPLY:
      case ND_DIVIDE:
      case ND_ADD:
      case ND_SUBTRACT:
        printf("\n");
        parse_print_tree(indent_level + 1, p_tree->nd_p_left_expr);
        parse_print_tree(indent_level + 1, p_tree->nd_p_right_expr);
        break;
      case ND_NEGATE:
        printf("\n");
        parse_print_tree(indent_level + 1, p_tree->nd_p_expr);
        break;
    }
  }
}

static void parse_expect(uint8_t lx_type_expected,
                         bool const skip_to_next  // Should we skip over the current lex unit to the next?
  )
{
  if (lx_type_expected != g_current_lex_unit.l_type) {
    fprintf(stderr, "%d:%d : Unexpected lexical unit %s.\n",
            g_input_line_n, g_input_column_n,
            g_lex_names[g_current_lex_unit.l_type]);
    exit(0);
  } else {
    lex_scan();
  }
}

static PARSE_NODE *parse_number(void)
{
  PARSE_NODE *retval = NULL;
  retval = MALLOC_1(PARSE_NODE);
  retval->nd_type = ND_NUMBER;
  retval->nd_number = g_current_lex_unit.l_number;
  lex_scan();  // Skip over numeric constant.
  return retval;
}

static PARSE_NODE *parse_variable_name(void)
{
  PARSE_NODE *retval = NULL;
  retval = MALLOC_1(PARSE_NODE);
  retval->nd_type = ND_VARIABLE;
  retval->nd_var_name = toupper(g_current_lex_unit.l_name[0]);  // Variable names are one character long.
  lex_scan();  // Skip over variable name.
  return retval;
}

// factor = variable-name | number | '(' expression ')'
static PARSE_NODE *parse_factor(void)
{
  PARSE_NODE *retval = NULL;
  switch (g_current_lex_unit.l_type) {
    case LX_IDENTIFIER:
      retval = parse_variable_name();
      break;
    case LX_NUMBER:
      retval = parse_number();
      break;
    default:
      fprintf(stderr, "%d:%d : Unexpected lexical unit %s.\n",
              g_input_line_n, g_input_column_n,
              g_lex_names[g_current_lex_unit.l_type]);
      exit(0);
      break;
  }
  return retval;
}

// term = factor (multiplicative-operator factor)*
static PARSE_NODE *parse_term(void)
{
  PARSE_NODE *retval = parse_factor();
  PARSE_NODE *new_root = NULL;
  uint8_t operator;
  while (LX_TIMES_SYM == g_current_lex_unit.l_type || LX_DIVIDE_SYM == g_current_lex_unit.l_type) {
    operator = LX_TIMES_SYM == g_current_lex_unit.l_type ? ND_MULTIPLY : ND_DIVIDE;
    lex_scan();  // Skip past '*' or '/'.
    new_root = MALLOC_1(PARSE_NODE);
    new_root->nd_type = operator;
    new_root->nd_p_left_expr = retval;
    new_root->nd_p_right_expr = parse_factor();
    retval = new_root;
  }
  return retval;
}


// expression = ['+'|'-'] term (addition-operator term)*
static PARSE_NODE *parse_expression(void)
{
  PARSE_NODE *retval = NULL;
  PARSE_NODE *new_root = NULL;
  uint8_t operator;              // addition operator ('+' | '-').
  int32_t sign_of_1st_term = 1;  // Is 1st term negated?
  while (LX_MINUS_SYM == g_current_lex_unit.l_type || LX_PLUS_SYM == g_current_lex_unit.l_type) {
    sign_of_1st_term *= LX_MINUS_SYM == g_current_lex_unit.l_type ? -1 : 1;
    lex_scan();  // Skip over '-' | '+'
  }
  retval = parse_term();
  while (LX_PLUS_SYM == g_current_lex_unit.l_type || LX_MINUS_SYM == g_current_lex_unit.l_type) {
    operator = LX_PLUS_SYM == g_current_lex_unit.l_type ? ND_ADD : ND_SUBTRACT;
    lex_scan();  // Skip past '+' or '-'.
    new_root = MALLOC_1(PARSE_NODE);
    new_root->nd_type = operator;
    new_root->nd_p_left_expr = retval;
    new_root->nd_p_right_expr = parse_term();
    retval = new_root;
  }
  if (sign_of_1st_term < 0) {
    new_root = MALLOC_1(PARSE_NODE);
    new_root->nd_type = ND_NEGATE;
    new_root->nd_p_expr = retval;
    retval = new_root;
  }
  return retval;
}

PARSE_NODE *parse(void)
{
  PARSE_NODE *retval;
  lex_scan(); // Get first lexical unit to start things going.
  retval = parse_expression();
  parse_expect(LX_EOF, false);
  return retval;
}
