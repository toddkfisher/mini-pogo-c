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
#include "compile.h"

//static uint8_t g_header[MAX_HEADER_SIZE];
//static uint32_t g_idx_header = 0;
static INSTRUCTION g_code[MAX_CODE_SIZE];
static uint32_t g_ip = 0;

typedef struct BACKPATCH
{
  uint32_t bp_addr;  // Where to backpatch.  i.e. index in g_code[] to set jump/spawn/.. address.
  struct BACKPATCH *bp_p_next;  // .. in list.
} BACKPATCH;

typedef struct LABEL
{
  char lbl_name[MAX_STR];
  bool lbl_addr_set;  // Is lbl_addr storing a valid address?
  uint32_t lbl_addr;
  BACKPATCH *lbl_p_backpatch_list;  // Backpatches for forward references to task names.
  struct LABEL *lbl_p_next;  // .. in hash bucket list.
} LABEL;

#define HTABLE_SIZE 4999
static LABEL *g_hash_table[HTABLE_SIZE];

static uint32_t compile_hash(char *s)
{
  uint32_t char_sum = 0;
  uint32_t result;
  while (*s)
  {
    char_sum += *s++;
  }
  result = char_sum % HTABLE_SIZE;
  return result;
}

LABEL *compile_lookup_label(char *label)
{
  LABEL *result;
  for (result = g_hash_table[compile_hash(label)];
       NULL != result && !STREQ(label, result->lbl_name);
       result = result->lbl_p_next)
  {
    // Empty loop body.  Everything happens in for loop header.
  }
  return result;
}

void compile_add_label(LABEL *p_label)
{
  uint32_t h = compile_hash(p_label->lbl_name);
  p_label->lbl_p_next = g_hash_table[h];
  g_hash_table[h] = p_label;
}

void compile_add_backpatch(LABEL *p_label, uint32_t addr)
{
  BACKPATCH *p_backpatch = malloc(sizeof(BACKPATCH));
  p_backpatch->bp_addr = addr;
  p_backpatch->bp_p_next = p_label->lbl_p_backpatch_list;
  p_label->lbl_p_backpatch_list = p_backpatch;
}

void compile_gen_END_TASK(void)
{
  g_code[g_ip++].i_opcode = OP_END_TASK;
}

void compile_binary_op(uint8_t opcode, PARSE_NODE *p_tree)
{
  compile(p_tree->nd_p_left_expr);
  compile(p_tree->nd_p_right_expr);
  g_code[g_ip++].i_opcode = opcode;
}

void compile_gen_BEGIN_SPAWN(void)
{
  g_code[g_ip++].i_opcode = OP_BEGIN_SPAWN;
}

void compile_gen_SPAWN(uint32_t task_addr)
{
  g_code[g_ip].i_opcode = OP_SPAWN;
  g_code[g_ip++].i_task_addr = task_addr;
}

void compile_gen_END_SPAWN(void)
{
  g_code[g_ip++].i_opcode = OP_END_SPAWN;
}

void compile_gen_PRINT_INT(void)
{
  g_code[g_ip++].i_opcode = OP_PRINT_INT;
}

void compile_gen_PRINT_CHAR(char ch)
{
  g_code[g_ip++].i_opcode = OP_PRINT_CHAR;
}

void compile_gen_PUSH_VAR(char var_name)
{
  g_code[g_ip++].i_opcode = OP_PUSH_VAR;
}

static void compile_ND_SPAWN(PARSE_NODE *p_nd_spawn)
{
  // "spawn t0; t1; t2; end"
  // compiles to:
  //     BEGIN_SPAWN
  //     SPAWN <addr of t0>
  //     SPAWN <addr of t1>
  //     SPAWN <addr of t2>
  //     END_SPAWN
  //
  // 1) If t_k isn't in the symbol table, then we add it and g_ip for future backpatching.
  // 2) If t_k is in the symbol table and it address is set (lbl_addr_set), then we use the address and no backpatching is necessary.
  // 3) If t_k is in the symbol table and its address is not set (lbl_addr_set), then we add it and g_ip++ for future backpatching.
  for (LISTITEM *p_task_name = p_nd_spawn->nd_p_task_names;
       NULL != p_task_name;
       p_task_name = p_task_name->l_p_next,
       g_ip += 1
    )
  {
    LABEL *p_label = compile_lookup_label(p_task_name->l_name);
    uint32_t task_addr = 0;
    if (NULL == p_label)
    {
      // Case 1.
      p_label = malloc(sizeof(LABEL));
      p_label->lbl_addr_set = false;
      p_label->lbl_p_backpatch_list = NULL;
      compile_add_label(p_label);
      compile_add_backpatch(p_label, g_ip);
    }
    else
    {
      if (p_label->lbl_addr_set)
      {
        // Case 2.
        task_addr = p_label->lbl_addr;
      }
      else
      {
        // Case 3.
        compile_add_backpatch(p_label, g_ip);
      }
    }
    compile_gen_SPAWN(task_addr);
  }
}

static void compile_ND_IF(PARSE_NODE *p_nd_if)
{
  uint32_t backpatch_0;
  uint32_t backpatch_1;
  // "if p then ss0 else ss1 end"
  // compiles to:
  //     compile(p)
  //     JUMP_IF_ZERO L0     ; <- backpatch_0 location
  //     compile(ss0)        ; could be empty
  //     JUMP L1             ; <- backpatch_1 location
  //   L0:                   ; address for jump instruction at backpatch_0
  //     compile(ss1)        ; could be empty
  //   L1:                   ; address for jump instruction at backpatch_1
  compile(p_nd_if->nd_p_if_test_expr);
  backpatch_0 = g_ip;
  g_code[g_ip++].i_opcode = OP_JUMP_IF_ZERO;
  compile(p_nd_if->nd_p_true_branch_statement_seq);
  backpatch_1 = g_ip;
  g_code[g_ip++].i_opcode = OP_JUMP;
  g_code[backpatch_0].i_jump_addr = g_ip;
  compile(p_nd_if->nd_p_false_branch_statement_seq);
  g_code[backpatch_1].i_jump_addr = g_ip;
}

static void compile_ND_WHILE(PARSE_NODE *p_nd_while)
{
  uint32_t top_of_loop_addr;
  uint32_t backpatch;
  // "while p do ss end"
  // compiles to:
  //     L0:                 ; <- top_of_loop_addr
  //       compile(p)
  //       JUMP_IF_ZERO L1   ; <- backpatch location
  //       compile(s)        ; could be empty
  //       JUMP L0
  //     L1:                 ; address for jump instruction at backpatch
  top_of_loop_addr = g_ip;
  compile(p_nd_while->nd_p_while_test_expr);
  backpatch = g_ip;
  g_code[g_ip++].i_opcode = OP_JUMP_IF_ZERO;
  compile(p_nd_while->nd_p_while_statement_seq);
  g_code[g_ip].i_opcode = OP_JUMP;
  g_code[g_ip++].i_jump_addr = top_of_loop_addr;
  g_code[backpatch].i_jump_addr = g_ip;
}

static void compile_OP_POP_INT(char var_name)
{
  g_code[g_ip].i_opcode = OP_POP_INT;
  g_code[g_ip++].i_var_name = var_name;
}

static void compile_OP_PRINT_CHAR(char ch)
{
  g_code[g_ip].i_opcode = OP_PRINT_CHAR;
  g_code[g_ip++].i_char = ch;
}

static void compile_OP_PUSH_CONST_INT(int32_t n)
{
  g_code[g_ip].i_opcode = OP_PUSH_CONST_INT;
  g_code[g_ip++].i_const_int = n;
}

static void compile_ND_PRINT_INT(PARSE_NODE *p_tree)
{
  compile(p_tree->nd_p_expr);
  g_code[g_ip++].i_opcode = OP_PRINT_INT;
}

static void compile_ND_NEGATE(PARSE_NODE *p_tree)
{
  compile(p_tree->nd_p_expr);
  g_code[g_ip++].i_opcode = OP_NEGATE;
}

static void compile_ND_STATEMENT_SEQUENCE(PARSE_NODE *p_tree)
{
  for (LISTITEM *p_statement = p_tree->nd_p_statement_seq;
       NULL != p_statement;
       p_statement = p_statement->l_p_next
    )
  {
    compile(p_statement->l_parse_node);
  }
}

void compile(PARSE_NODE *p_tree)
{
  if (NULL != p_tree)
  {
    switch (p_tree->nd_type)
    {
      case ND_MODULE_DECLARATION:
        break;
      case ND_TASK_DECLARATION:
        break;
      case ND_STATEMENT_SEQUENCE:
        break;
      case ND_ASSIGN:
        compile_OP_POP_INT(p_tree->nd_var_name);
        break;
      case ND_IF:
        compile_ND_IF(p_tree);
        break;
      case ND_WHILE:
        compile_ND_WHILE(p_tree);
        break;
      case ND_PRINT_INT:
        compile_ND_PRINT_INT();
        break;
      case ND_PRINT_CHAR:
        compile_OP_PRINT_CHAR(p_tree->nd_char);
        break;
      case ND_SPAWN:
        compile_ND_SPAWN(p_tree);
        break;
      case ND_STOP:
        compile_gen_END_TASK();
        break;
      case ND_OR:
        compile_binary_op(OP_OR, p_tree);
        break;
      case ND_AND:
        compile_binary_op(OP_AND, p_tree);
        break;
      case ND_NOT:
        compile_binary_op(OP_NOT, p_tree);
        break;
      case ND_LE:
        compile_binary_op(OP_LE, p_tree);
        break;
      case ND_LT:
        compile_binary_op(OP_LT, p_tree);
        break;
      case ND_GE:
        compile_binary_op(OP_GE, p_tree);
        break;
      case ND_GT:
        compile_binary_op(OP_GT, p_tree);
        break;
      case ND_EQ:
        compile_binary_op(OP_EQ, p_tree);
        break;
      case ND_NE:
        compile_binary_op(OP_NE, p_tree);
        break;
      case ND_NEGATE:
        compile_ND_NEGATE(p_tree);
        break;
      case ND_ADD:
        compile_binary_op(OP_ADD, p_tree);
        break;
      case ND_SUBTRACT:
        compile_binary_op(OP_SUBTRACT, p_tree);
        break;
      case ND_MULTIPLY:
        compile_binary_op(OP_MULTIPLY, p_tree);
        break;
      case ND_DIVIDE:
        compile_binary_op(OP_DIVIDE, p_tree);
        break;
      case ND_VARIABLE:
        compile_gen_PUSH_VAR(p_tree->nd_var_name);
        break;
      case ND_NUMBER:
        compile_OP_PUSH_CONST_INT(p_tree->nd_number);
        break;
      default:
        break;
    }
  }
}
