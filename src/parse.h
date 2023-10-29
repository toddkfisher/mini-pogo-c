#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <util.h>

#include <enum-int.h>
enum PARSE_NODE_TYPES
{
#include "parse-node-enum.txt"
};

typedef struct PARSE_NODE PARSE_NODE;

typedef struct LISTITEM
{
  struct LISTITEM *l_p_next;
  union
  {
    PARSE_NODE *l_parse_node;
    char l_name[MAX_STR];
  };
} LISTITEM;

struct PARSE_NODE
{
  uint8_t nd_type;
  union
  {
    // ND_MODULE
    struct
    {
      char nd_module_name[MAX_STR];
      LISTITEM *nd_p_task_decl_list;
    };
    // ND_TASK_DECLARATION
    struct
    {
      char nd_task_name[MAX_STR];
      PARSE_NODE *nd_p_task_body;
    };
    // ND_STATEMENT_SEQUENCE
    LISTITEM *nd_p_statement_seq;
    // ND_ASSIGN
    struct
    {
      char nd_var_name;
      PARSE_NODE *nd_p_assign_expr;
    };
    // ND_IF
    struct
    {
      PARSE_NODE *nd_p_if_test_expr;
      PARSE_NODE *nd_p_true_branch_statement_seq;
      PARSE_NODE *nd_p_false_branch_statement_seq;
    };
    // ND_WHILE
    struct
    {
      PARSE_NODE *nd_p_while_test_expr;
      PARSE_NODE *nd_p_while_statement_seq;
    };
    // ND_PRINT_CHAR
    char nd_char;
    // ND_PRINT_INT, ND_NOT, ND_NEGATE
    PARSE_NODE *nd_p_expr;
    // ND_SPAWN
    LISTITEM *nd_p_task_names;
    // ND_OR, ND_AND
    // ND_LE, ND_LT, ND_GE, ND_GT, ND_EQ, ND_NE
    // ND_ADD, ND_SUBTRACT, ND_MULTIPLY, ND_DIVIDE
    struct
    {
      PARSE_NODE *nd_p_left_expr;
      PARSE_NODE *nd_p_right_expr;
    };
    // ND_NUMBER
    int32_t nd_number;
    // ND_STOP (no subtree(s) or other infotainment).
  };
};

PARSE_NODE *parse(void);
void parse_print_tree(uint32_t indent_level, PARSE_NODE *const p_tree);
