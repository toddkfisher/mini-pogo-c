#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
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
extern bool g_lex_debug_print;
extern char *ASCII[];

#include <enum-str.h>
char *g_parse_node_names[] =
{
#include "parse-node-enum.txt"
};

#define N_SPACES_INDENT 1

static void parse_print_indent(uint32_t indent_level, char indent_char)
{
  for (uint32_t i = 0; i < indent_level; ++i)
    for (uint32_t j = 0; j < N_SPACES_INDENT; ++j)
      putchar(indent_char);
}

void parse_print_tree(uint32_t indent_level, PARSE_NODE *const p_tree)
{
  if (NULL != p_tree)
  {
    parse_print_indent(indent_level, '*');
    printf(" %s: ", g_parse_node_names[p_tree->nd_type]);
    switch (p_tree->nd_type)
    {
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
      case ND_ASSIGN:
        printf("%c\n", p_tree->nd_var_name);
        parse_print_tree(indent_level + 1, p_tree->nd_p_assign_expr);
        break;
      case ND_STATEMENT_SEQUENCE:
        printf("\n");
        for (LISTITEM *p_statement = p_tree->nd_p_statement_seq;
             NULL != p_statement;
             p_statement = p_statement->l_p_next)
          parse_print_tree(indent_level + 1, p_statement->l_parse_node);
        break;
      case ND_IF:
        printf("\n");
        parse_print_tree(indent_level + 1, p_tree->nd_p_if_test_expr);
        parse_print_tree(indent_level + 1, p_tree->nd_p_true_branch_statement_seq);
        parse_print_tree(indent_level + 1, p_tree->nd_p_false_branch_statement_seq);
        break;
      case ND_WHILE:
        printf("\n");
        parse_print_tree(indent_level + 1, p_tree->nd_p_while_test_expr);
        parse_print_tree(indent_level + 1, p_tree->nd_p_true_branch_statement_seq);
        break;
      case ND_SPAWN:
        for (LISTITEM *p_name = p_tree->nd_p_task_names; NULL != p_name; p_name = p_name->l_p_next)
        {
          printf("%s", p_name->l_name);
          if (NULL != p_name->l_p_next)
            printf(", ");
        }
        printf("\n");
        break;
      case ND_PRINT_CHAR:
        printf("%s\n", ASCII[(uint8_t) p_tree->nd_char]);
        break;
      case ND_PRINT_INT:
        printf("\n");
        parse_print_tree(indent_level + 1, p_tree->nd_p_expr);
        break;
      case ND_STOP:
        printf("\n");
        break;
      case ND_TASK_DECLARATION:
        printf("%s\n", p_tree->nd_task_name);
        parse_print_tree(indent_level + 1, p_tree->nd_p_task_body);
        break;
      case ND_MODULE_DECLARATION:
        printf("%s\n", p_tree->nd_module_name);
        parse_print_tree(indent_level + 1, p_tree->nd_p_init_statements);
        for (LISTITEM *p_task_decl = p_tree->nd_p_task_decl_list;
             NULL != p_task_decl;
             p_task_decl = p_task_decl->l_p_next)
        {
          parse_print_tree(indent_level + 1, p_task_decl->l_parse_node);
        }
        break;
      default:
        printf("Unhandled node type in parse_print_tree()\n");
        break;
    }
  }
}

static void parse_optional(uint8_t lx_type)
{
  if (lx_type == g_current_lex_unit.l_type)
    lex_scan();
}

static void parse_expect(uint8_t lx_type_expected,
                         bool const skip_to_next  // Should we skip over the current lex unit to the next?
  )
{
  if (lx_type_expected != g_current_lex_unit.l_type)
  {
    fprintf(stderr, "%d:%d : Unexpected lexical unit.\n",
            g_input_line_n, g_input_column_n);
    lex_print(&g_current_lex_unit);
    fprintf(stderr, "%s expected.\n", g_lex_names[lx_type_expected]);
    error_exit(0);
  }
  else if (skip_to_next)
    lex_scan();
}

static PARSE_NODE *parse_number(void)
{
  PARSE_NODE *retval = NULL;
  retval = malloc(sizeof(PARSE_NODE));
  retval->nd_type = ND_NUMBER;
  retval->nd_number = g_current_lex_unit.l_number;
  lex_scan();  // Skip over numeric constant.
  return retval;
}

static PARSE_NODE *parse_variable_name(void)
{
  PARSE_NODE *retval = NULL;
  retval = malloc(sizeof(PARSE_NODE));
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
  switch (g_current_lex_unit.l_type)
  {
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
      error_exit(0);
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
  while (LX_TIMES_SYM == g_current_lex_unit.l_type ||
         LX_DIVIDE_SYM == g_current_lex_unit.l_type ||
         LX_REMAINDER_SYM == g_current_lex_unit.l_type)
  {
    switch (g_current_lex_unit.l_type)
    {
      case LX_TIMES_SYM:
        operator = ND_MULTIPLY;
        break;
      case LX_DIVIDE_SYM:
        operator = ND_DIVIDE;
        break;
      case LX_REMAINDER_SYM:
        operator = ND_REMAINDER;
        break;
    }
    lex_scan();  // Skip past '*' or '%' or '/'.
    p_new_root = malloc(sizeof(PARSE_NODE));
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
  while (LX_MINUS_SYM == g_current_lex_unit.l_type
         || LX_PLUS_SYM == g_current_lex_unit.l_type)
  {
    sign_of_1st_term *= LX_MINUS_SYM == g_current_lex_unit.l_type ? -1 : 1;
    lex_scan();  // Skip over '-' | '+'
  }
  retval = parse_term();
  if (sign_of_1st_term < 0)
  {
    p_negation = malloc(sizeof(PARSE_NODE));
    p_negation->nd_type = ND_NEGATE;
    p_negation->nd_p_expr = retval;
    retval = p_negation;
  }
  while (LX_PLUS_SYM == g_current_lex_unit.l_type
         || LX_MINUS_SYM == g_current_lex_unit.l_type)
  {
    operator = LX_PLUS_SYM == g_current_lex_unit.l_type ? ND_ADD : ND_SUBTRACT;
    lex_scan();  // Skip past '+' or '-'.
    p_new_root = malloc(sizeof(PARSE_NODE));
    p_new_root->nd_type = operator;
    p_new_root->nd_p_left_expr = retval;
    p_new_root->nd_p_right_expr = parse_term();
    retval = p_new_root;
  }
  return retval;
}

static bool parse_is_comparison_operator(uint8_t lex_type)
{
  return lex_type > LX_COMPARISON_SYMBOL_BEGIN &&
    lex_type < LX_COMPARISON_SYMBOL_END;
}

// comparison-expression = expression [relation-operator expression]
static PARSE_NODE *parse_comparison_expression(void)
{
  PARSE_NODE *retval = parse_expression();
  PARSE_NODE *p_new_root = NULL;
  if (parse_is_comparison_operator(g_current_lex_unit.l_type))
  {
    p_new_root = malloc(sizeof(PARSE_NODE));
    switch (g_current_lex_unit.l_type)
    {
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
static PARSE_NODE *parse_and_expression(void)
{
  PARSE_NODE *retval = NULL;
  PARSE_NODE *p_new_root = NULL;
  PARSE_NODE *p_negation = NULL;
  bool has_leading_not_kw = false;
  while (LX_NOT_KW == g_current_lex_unit.l_type)
  {
    has_leading_not_kw = !has_leading_not_kw;
    lex_scan();  // Skip past 'not'.
  }
  retval =  parse_comparison_expression();
  if (has_leading_not_kw)
  {
    p_negation = malloc(sizeof(PARSE_NODE));
    p_negation->nd_type = ND_NOT;
    p_negation->nd_p_expr = retval;
    retval = p_negation;
  }
  while (LX_AND_KW == g_current_lex_unit.l_type)
  {
    lex_scan();  // Skip past 'and'.
    p_new_root = malloc(sizeof(PARSE_NODE));
    p_new_root->nd_type = ND_AND;
    p_new_root->nd_p_left_expr = retval;
    p_new_root->nd_p_right_expr = parse_comparison_expression();
    retval = p_new_root;
  }
  return retval;
}

// or-expression = and-expression ('or' and-expression)*
static PARSE_NODE *parse_or_expression(void)
{
  PARSE_NODE *retval = NULL;
  PARSE_NODE *p_new_root = NULL;
  retval =  parse_and_expression();
  while (LX_OR_KW == g_current_lex_unit.l_type)
  {
    lex_scan();  // Skip past 'or'.
    p_new_root = malloc(sizeof(PARSE_NODE));
    p_new_root->nd_type = ND_OR;
    p_new_root->nd_p_left_expr = retval;
    p_new_root->nd_p_right_expr = parse_and_expression();
    retval = p_new_root;
  }
  return retval;
}

// assignment-statement = variable-name ':=' or-expression
static PARSE_NODE *parse_assignment(void)
{
  PARSE_NODE *result = malloc(sizeof(PARSE_NODE));
  result->nd_type = ND_ASSIGN;
  result->nd_var_name = toupper(g_current_lex_unit.l_name[0]);
  lex_scan();  // Skip over variable name.
  parse_expect(LX_ASSIGN_SYM, true);
  result->nd_p_assign_expr = parse_or_expression();
  return result;
}

static PARSE_NODE *parse_statement(void);

// statement-sequence = (statement ';')*
static PARSE_NODE *parse_statement_sequence(void)
{
  PARSE_NODE *result = malloc(sizeof(PARSE_NODE));
  LISTITEM *p_statement = NULL;
  LISTITEM *p_prev_statement = NULL;
  result->nd_type = ND_STATEMENT_SEQUENCE;
  result->nd_p_statement_seq = NULL;
  // If current lex unit is a follower of statment-squence then we have
  // zero instances of statement.  Example:
  // if p then
  //   ! empty statement-sequence.
  // else
  //   x := 666;
  // end
  while (LX_ELSE_KW != g_current_lex_unit.l_type && LX_END_KW != g_current_lex_unit.l_type) {
    p_statement = malloc(sizeof(LISTITEM));
    p_statement->l_p_next = NULL;
    p_statement->l_parse_node = parse_statement();
    parse_expect(LX_SEMICOLON_SYM, true);
    if (NULL == result->nd_p_statement_seq)
      result->nd_p_statement_seq = p_statement;
    else
      p_prev_statement->l_p_next = p_statement; // test commend
    p_prev_statement = p_statement;
  }
  return result;
}

// if-statement = 'if' expression 'then' statement-sequence
//                 ['else' statement-sequence] 'end'
static PARSE_NODE *parse_if(void)
{
  PARSE_NODE *result = malloc(sizeof(PARSE_NODE));
  lex_scan();  // Skip over 'if'.
  result->nd_type = ND_IF;
  result->nd_p_if_test_expr = parse_or_expression();
  parse_expect(LX_THEN_KW, true);
  result->nd_p_true_branch_statement_seq = parse_statement_sequence();
  if (LX_ELSE_KW == g_current_lex_unit.l_type)
  {
    lex_scan();  // Skip over 'else'
    result->nd_p_false_branch_statement_seq = parse_statement_sequence();
  }
  else
    result->nd_p_false_branch_statement_seq = NULL;
  parse_expect(LX_END_KW, true);
  return result;
}

static PARSE_NODE *parse_while(void)
{
  PARSE_NODE *result = malloc(sizeof(PARSE_NODE));
  result->nd_type = ND_WHILE;
  lex_scan();  // Skip over 'while'.
  result->nd_p_while_test_expr = parse_or_expression();
  parse_expect(LX_DO_KW, true);
  result->nd_p_while_statement_seq = parse_statement_sequence();
  parse_expect(LX_END_KW, true);
  return result;
}

// spawn-statement = 'spawn' (name ';')+ 'end'
static PARSE_NODE *parse_spawn(void)
{
  PARSE_NODE *result = malloc(sizeof(PARSE_NODE));
  LISTITEM *p_current_name;
  LISTITEM *p_prev_name;
  lex_scan();   // Skip over 'spawn'.
  result->nd_type = ND_SPAWN;
  result->nd_p_task_names = NULL;
  do
  {
    parse_expect(LX_IDENTIFIER, false);
    p_current_name = malloc(sizeof(LISTITEM));
    p_current_name->l_p_next = NULL;
    strncpy(p_current_name->l_name, g_current_lex_unit.l_name, MAX_STR - 1);
    lex_scan();  // Skip past name.
    parse_expect(LX_SEMICOLON_SYM, true);
    if (NULL == result->nd_p_task_names)
      result->nd_p_task_names = p_current_name;
    else
      p_prev_name->l_p_next = p_current_name;
    p_prev_name = p_current_name;
  } while (LX_END_KW != g_current_lex_unit.l_type);
  lex_scan();  // Skip past 'end'.
  return result;
}

// print-char-statement = 'print_char' character-constant
static PARSE_NODE *parse_print_char(void)
{
  PARSE_NODE *result = malloc(sizeof(PARSE_NODE));
  lex_scan();  // Skip past 'print_char'.
  parse_expect(LX_CHAR, false);
  result->nd_type = ND_PRINT_CHAR;
  result->nd_char = g_current_lex_unit.l_char;
  lex_scan();  // Skip over character constant.
  return result;
}

// stop-statement = 'stop'
static PARSE_NODE *parse_stop(void)
{
  PARSE_NODE *result = malloc(sizeof(PARSE_NODE));
  lex_scan();  // Skip past 'stop'.
  result->nd_type = ND_STOP;
  return result;
}

// print-int-statement = 'print_int' expression
static PARSE_NODE *parse_print_int(void)
{
  PARSE_NODE *result = malloc(sizeof(PARSE_NODE));
  lex_scan();  // Skip past 'print_int'.
  result->nd_type = ND_PRINT_INT;
  result->nd_p_expr = parse_or_expression();
  return result;
}

// statement =
//             assignment-statement
//           | if-statement
//           | while-statement
//           | stop-statement
//           | spawn-statement
//           | print-int-statement
//           | print-char-statement
static PARSE_NODE *parse_statement(void)
{
  PARSE_NODE *result = NULL;
  switch (g_current_lex_unit.l_type)
  {
    case LX_IDENTIFIER:
      result = parse_assignment();
      break;
    case LX_IF_KW:
      result = parse_if();
      break;
    case LX_WHILE_KW:
      result = parse_while();
      break;
    case LX_SPAWN_KW:
      result = parse_spawn();
      break;
  case LX_STOP_KW:
      result = parse_stop();
      break;
  case LX_PRINT_INT_KW:
      result = parse_print_int();
      break;
    case LX_PRINT_CHAR_KW:
      result = parse_print_char();
      break;
    default:
      break;
  }
  return result;
}

// task-declaration = 'task' name [';'] statement-sequence 'end'
static PARSE_NODE *parse_task_declaration(void)
{
  PARSE_NODE *result = malloc(sizeof(PARSE_NODE));
  result->nd_type = ND_TASK_DECLARATION;
  parse_expect(LX_TASK_KW, true);
  parse_optional(LX_SEMICOLON_SYM);
  parse_expect(LX_IDENTIFIER, false);
  strcpy(result->nd_task_name, g_current_lex_unit.l_name);
  lex_scan();  // Skip name.
  result->nd_p_task_body = parse_statement_sequence();
  parse_expect(LX_END_KW, true);
  return result;
}

// module-declaration  =
//                       'module' name [';']
//                       'init' statement-sequence 'end' ';'
//                       (task-declaration ';')* 'end'
static PARSE_NODE *parse_module_declaration(void)
{
  PARSE_NODE *result = malloc(sizeof(PARSE_NODE));
  LISTITEM *p_current_task_decl = NULL;
  LISTITEM *p_prev_task_decl = NULL;
  result->nd_type = ND_MODULE_DECLARATION;
  parse_expect(LX_MODULE_KW, true);
  parse_expect(LX_IDENTIFIER, false);
  strcpy(result->nd_module_name, g_current_lex_unit.l_name);
  lex_scan();  // Skip over name.
  parse_optional(LX_SEMICOLON_SYM);
  parse_expect(LX_INIT_KW, true);
  result->nd_p_init_statements = parse_statement_sequence();
  parse_expect(LX_END_KW, true);
  parse_optional(LX_SEMICOLON_SYM);
  result->nd_p_task_decl_list = NULL;
  while (LX_END_KW != g_current_lex_unit.l_type)
  {
    p_current_task_decl = malloc(sizeof(LISTITEM));
    p_current_task_decl->l_parse_node = parse_task_declaration();
    p_current_task_decl->l_p_next = NULL;
    parse_optional(LX_SEMICOLON_SYM);
    if (NULL == result->nd_p_task_decl_list)
      result->nd_p_task_decl_list = p_current_task_decl;
    else
      p_prev_task_decl->l_p_next = p_current_task_decl;
    p_prev_task_decl = p_current_task_decl;
  }
  lex_scan();  // Skip 'end' keyword.
  parse_optional(LX_SEMICOLON_SYM);
  return result;
}

PARSE_NODE *parse(void)
{
  PARSE_NODE *retval;
  lex_scan(); // Get first lexical unit to start things going.
  retval = parse_module_declaration();
  parse_expect(LX_EOF, false);
  return retval;
}
