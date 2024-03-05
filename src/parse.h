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


// List of random stuff living in PARSE_NODEs
typedef struct LISTITEM LISTITEM;
struct LISTITEM
{
  LISTITEM *l_p_next;
  union
  {
    PARSE_NODE *l_parse_node;
    char l_name[MAX_STR];
    char l_string[MAX_STR];
  };
};


struct PARSE_NODE
{
  uint8_t nd_type;
  uint32_t nd_src_line;
  uint32_t nd_src_col;
  union
  {
    // nd_type == ND_MODULE
    struct
    {
      char nd_module_name[MAX_STR];
      PARSE_NODE *nd_p_init_statements;
      LISTITEM *nd_p_task_decl_list;
    };
    // nd_type ==  ND_TASK_DECLARATION
    struct
    {
      char nd_task_name[MAX_STR];
      PARSE_NODE *nd_p_task_body;
    };
    //  nd_type == ND_STATEMENT_SEQUENCE
    //  Each LISTITEM holds a mini-pogo statement parse tree.
    LISTITEM *nd_p_statement_seq;
    //  nd_type == D_ATOMIC_PRINT
    // Each LISTITEM holds either an ND_PRINT_STRING or an ND_PRINT_INT
    LISTITEM *nd_p_print_items;
    // nd_type ==  ND_PRINT_STRING
    char nd_string_const[MAX_STR];
    //  nd_type == ND_ASSIGN
    struct
    {
      char nd_var_name;
      PARSE_NODE *nd_p_assign_expr;
    };
    //  nd_type == ND_IF
    struct
    {
      PARSE_NODE *nd_p_if_test_expr;
      PARSE_NODE *nd_p_true_branch_statement_seq;
      PARSE_NODE *nd_p_false_branch_statement_seq;
    };
    //  nd_type == ND_WHILE
    struct
    {
      PARSE_NODE *nd_p_while_test_expr;
      PARSE_NODE *nd_p_while_statement_seq;
    };
    //  nd_type == ND_PRINT_CHAR
    char nd_char;
    //  nd_type == ND_PRINT_INT, ND_NOT, ND_NEGATE, ND_SLEEP
    PARSE_NODE *nd_p_expr;
    //  nd_type == ND_SPAWN_JOIN, ND_SPAWN_JOIN_WITH_TIMEOUT
    struct
    {
      LISTITEM *nd_p_task_names;
      //  nd_type == ND_SPAWN_JOIN_WITH_TIMEOUT
      PARSE_NODE *nd_p_millisec_expr;
      PARSE_NODE *nd_p_statement_seq_if_timed_out;
      PARSE_NODE *nd_p_statement_seq_if_not_timed_out;
    };
    //  nd_type == ND_OR, ND_AND
    //             ND_LE, ND_LT,
    //             ND_GE, ND_GT,
    //             ND_EQ, ND_NE
    //             ND_ADD, ND_SUBTRACT,
    //             ND_MULTIPLY, ND_DIVIDE,
    //             ND_REMAINDER
    struct
    {
      PARSE_NODE *nd_p_left_expr;
      PARSE_NODE *nd_p_right_expr;
    };
    // nd_type ==  ND_NUMBER
    int32_t nd_number;
    //  nd_type == ND_STOP (no subtree(s) or other infotainment).
  };
};


PARSE_NODE *parse(void);
void parse_print_tree(uint32_t indent_level, PARSE_NODE *const p_tree);
