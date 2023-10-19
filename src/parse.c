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
      case ND_LE:
      case ND_LT:
      case ND_GE:
      case ND_GT:
      case ND_EQ:
      case ND_NE:
      case ND_AND:
      case ND_OR:
        printf("\n");
        parse_print_tree(indent_level + 1, p_tree->nd_p_left_expr);
        parse_print_tree(indent_level + 1, p_tree->nd_p_right_expr);
        break;
      case ND_NEGATE:
      case ND_NOT:
        printf("\n");
        parse_print_tree(indent_level + 1, p_tree->nd_p_expr);
        break;
      default:
        printf("???\n");
        break;
    }
  }
}

static void parse_expect(uint8_t lx_type_expected,
                         bool const skip_to_next  // Should we skip over the current lex unit to the next?
  )
{
  if (lx_type_expected != g_current_lex_unit.l_type) {
    fprintf(stderr, "%d:%d : Unexpected lexical unit.\n",
            g_input_line_n, g_input_column_n);
    lex_print(&g_current_lex_unit);
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

static PARSE_NODE *parse_or_expression(void);

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
    case LX_LPAREN_SYM:
      lex_scan();  // Skip over '('
      retval = parse_or_expression();
      parse_expect(LX_RPAREN_SYM, true);
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
  PARSE_NODE *p_new_root = NULL;
  uint8_t operator;
  while (LX_TIMES_SYM == g_current_lex_unit.l_type || LX_DIVIDE_SYM == g_current_lex_unit.l_type) {
    operator = LX_TIMES_SYM == g_current_lex_unit.l_type ? ND_MULTIPLY : ND_DIVIDE;
    lex_scan();  // Skip past '*' or '/'.
    p_new_root = MALLOC_1(PARSE_NODE);
    p_new_root->nd_type = operator;
    p_new_root->nd_p_left_expr = retval;
    p_new_root->nd_p_right_expr = parse_factor();
    retval = p_new_root;
  }
  return retval;
}


// expression = ['+'|'-'] term (addition-operator term)*
static PARSE_NODE *parse_expression(void)
{
  PARSE_NODE *retval = NULL;
  PARSE_NODE *p_new_root = NULL;
  PARSE_NODE *p_negation = NULL;
  uint8_t operator;              // addition operator ('+' | '-').
  int32_t sign_of_1st_term = 1;  // Is 1st term negated?
  while (LX_MINUS_SYM == g_current_lex_unit.l_type || LX_PLUS_SYM == g_current_lex_unit.l_type) {
    sign_of_1st_term *= LX_MINUS_SYM == g_current_lex_unit.l_type ? -1 : 1;
    lex_scan();  // Skip over '-' | '+'
  }
  retval = parse_term();
  if (sign_of_1st_term < 0) {
    p_negation = MALLOC_1(PARSE_NODE);
    p_negation->nd_type = ND_NEGATE;
    p_negation->nd_p_expr = retval;
    retval = p_negation;
  }
  while (LX_PLUS_SYM == g_current_lex_unit.l_type || LX_MINUS_SYM == g_current_lex_unit.l_type) {
    operator = LX_PLUS_SYM == g_current_lex_unit.l_type ? ND_ADD : ND_SUBTRACT;
    lex_scan();  // Skip past '+' or '-'.
    p_new_root = MALLOC_1(PARSE_NODE);
    p_new_root->nd_type = operator;
    p_new_root->nd_p_left_expr = retval;
    p_new_root->nd_p_right_expr = parse_term();
    retval = p_new_root;
  }
  return retval;
}

bool parse_is_comparison_operator(uint8_t lex_type)
{
  return lex_type > LX_COMPARISON_SYMBOL_BEGIN &&
    lex_type < LX_COMPARISON_SYMBOL_END;
}

// comparison-expression = expression [relation-operator expression]
PARSE_NODE *parse_comparison_expression(void)
{
  PARSE_NODE *retval = parse_expression();
  PARSE_NODE *p_new_root = NULL;
  if (parse_is_comparison_operator(g_current_lex_unit.l_type)) {
    p_new_root = MALLOC_1(PARSE_NODE);
    switch (g_current_lex_unit.l_type) {
      case LX_EQ_SYM:
        p_new_root->nd_type = ND_EQ;
        break;
      case LX_NE_SYM:
        p_new_root->nd_type = ND_NE;
        break;
      case LX_GE_SYM:
        p_new_root->nd_type = ND_GE;
        break;
      case LX_GT_SYM:
        p_new_root->nd_type = ND_GT;
        break;
      case LX_LT_SYM:
        p_new_root->nd_type = ND_LT;
        break;
      case LX_LE_SYM:
        p_new_root->nd_type = ND_LE;
        break;
    }
    lex_scan();  // Skip over relation-operator.
    p_new_root->nd_p_left_expr = retval;
    p_new_root->nd_p_right_expr = parse_expression();
    retval = p_new_root;
  }
  return retval;
}

// and-expression = ['not'] comparison-expression ('and' comparison-expression)*
PARSE_NODE *parse_and_expression(void)
{
  PARSE_NODE *retval = NULL;
  PARSE_NODE *p_new_root = NULL;
  PARSE_NODE *p_negation = NULL;
  bool has_leading_not_kw = false;
  while (LX_NOT_KW == g_current_lex_unit.l_type) {
    has_leading_not_kw = !has_leading_not_kw;
    lex_scan();  // Skip past 'not'.
  }
  retval =  parse_comparison_expression();
  if (has_leading_not_kw) {
    p_negation = MALLOC_1(PARSE_NODE);
    p_negation->nd_type = ND_NOT;
    p_negation->nd_p_expr = retval;
    retval = p_negation;
  }
  while (LX_AND_KW == g_current_lex_unit.l_type) {
    lex_scan();  // Skip past 'and'.
    p_new_root = MALLOC_1(PARSE_NODE);
    p_new_root->nd_type = ND_AND;
    p_new_root->nd_p_left_expr = retval;
    p_new_root->nd_p_right_expr = parse_comparison_expression();
    retval = p_new_root;
  }
  return retval;
}

// or-expression = and-expression ('or' and-expression)*
PARSE_NODE *parse_or_expression(void)
{
  PARSE_NODE *retval = NULL;
  PARSE_NODE *p_new_root = NULL;
  retval =  parse_and_expression();
  while (LX_OR_KW == g_current_lex_unit.l_type) {
    lex_scan();  // Skip past 'or'.
    p_new_root = MALLOC_1(PARSE_NODE);
    p_new_root->nd_type = ND_OR;
    p_new_root->nd_p_left_expr = retval;
    p_new_root->nd_p_right_expr = parse_and_expression();
    retval = p_new_root;
  }
  return retval;
}

PARSE_NODE *parse(void)
{
  PARSE_NODE *retval;
  lex_scan(); // Get first lexical unit to start things going.
  retval = parse_or_expression();
  parse_expect(LX_EOF, false);
  return retval;
}
