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
#include "instruction.h"
#include "compile.h"

// header_size : u32
// n_labels    : u32
// module_name : counted_string
// label_list  : struct
//              {
//                lbl_name : counted_string
//                lbl_type : u8
//                lbl_addr : u32 // relative to 0th instruction of code.
//              } [n_labels]

#define HEADER_SIZE_IDX 0
#define HEADER_N_LABELS_IDX (sizeof(uint32_t))
static uint8_t g_header[MAX_HEADER_SIZE];
static uint32_t g_idx_header = 0;  // Current location in header we are writing to.
static uint32_t g_n_labels = 0;
static INSTRUCTION g_code[MAX_CODE_SIZE];
static uint32_t g_ip = 0;

typedef struct BACKPATCH
{
  uint32_t bp_addr;  // Where to backpatch.  i.e. index in g_code[] to set jump/spawn/.. address.
  struct BACKPATCH *bp_p_next;  // Next BACKPATCH in list.
} BACKPATCH;

typedef struct LABEL
{
  char lbl_name[MAX_STR];
  bool lbl_addr_set;  // Is lbl_addr storing a valid address?
  uint32_t lbl_addr;
  bool lbl_is_task;  // Jump label or location of task?
  BACKPATCH *lbl_p_backpatch_list;  // Backpatches for forward references to task names.
  struct LABEL *lbl_p_next;  // next LABEL in hash bucket list.
} LABEL;

#define HTABLE_SIZE 4999
static LABEL *g_hash_table[HTABLE_SIZE];
static uint32_t g_label_suffix_number = 0;

static void compile_add_u32_to_header(uint32_t u32)
{
  memcpy(&g_header[g_idx_header], &u32, sizeof(u32));
  g_idx_header += sizeof(u32);
}

// Format of counted string: [u32 (bytes)][byte0][byte1]...
static void compile_add_counted_string_to_header(char *str)
{
  uint32_t n_bytes = strlen(str);
  compile_add_u32_to_header(n_bytes);
  memcpy(&g_header[g_idx_header], str, n_bytes);
  g_idx_header += n_bytes;
}

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

void compile_create_label_name(char *prefix, char *label_name)
{
  sprintf(label_name, "%s%u", prefix, g_label_suffix_number++);
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

static LABEL *compile_add_label(char *name,
                                bool addr_set,
                                uint32_t addr,
                                bool is_task
  )
{
  uint32_t h = compile_hash(name);
  LABEL *result = malloc(sizeof(LABEL));
  strcpy(result->lbl_name, name);
  result->lbl_addr_set = false;
  result->lbl_addr = addr;
  result->lbl_is_task = true;
  result->lbl_p_backpatch_list = NULL;
  result->lbl_p_next = g_hash_table[h];
  g_hash_table[h] = result;
  return result;
}

static LABEL *compile_add_task_label(char *task_name, uint32_t task_addr)
{
  LABEL *result = compile_add_label(task_name, /*addr_set*/ true, task_addr, /*is_task*/ true);
  return result;
}

static LABEL *compile_add_forward_ref_task_label(char *task_name)
{
  LABEL *result = compile_add_label(task_name, /*addr_set*/ false, /*addr*/ 0, /*is_task*/ true);
  return result;
}

static LABEL *compile_add_jump_label(char *label_name, uint32_t jump_addr)
{
  LABEL *result = compile_add_label(label_name, /*addr_set*/ true, jump_addr, /*is_task*/ false);
  return result;
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
      compile_add_forward_ref_task_label(p_task_name->l_name);
      g_n_labels += 1;
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
  char jump_label_name[MAX_STR];
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
    // Add L0 for "disassembler"
  compile_create_label_name("IF_L0", jump_label_name);
  compile_add_jump_label(jump_label_name, g_ip);
  g_n_labels += 1;
  compile(p_nd_if->nd_p_false_branch_statement_seq);
  g_code[backpatch_1].i_jump_addr = g_ip;
    // Add L1 for "disassembler"
  compile_create_label_name("IF_L1", jump_label_name);
  compile_add_jump_label(jump_label_name, g_ip);
  g_n_labels += 1;
}

static void compile_ND_WHILE(PARSE_NODE *p_nd_while)
{
  uint32_t top_of_loop_addr;
  uint32_t backpatch;
  char jump_label_name[MAX_STR];
  // "while p do ss end"
  // compiles to:
  //     L0:                 ; <- top_of_loop_addr
  //       compile(p)
  //       JUMP_IF_ZERO L1   ; <- backpatch location
  //       compile(s)        ; could be empty
  //       JUMP L0
  //     L1:                 ; address for jump instruction at backpatch
  top_of_loop_addr = g_ip;
  compile_create_label_name("WHILE_L0", jump_label_name);
  compile_add_jump_label(jump_label_name, g_ip);
  g_n_labels += 1;
  compile(p_nd_while->nd_p_while_test_expr);
  backpatch = g_ip;
  g_code[g_ip++].i_opcode = OP_JUMP_IF_ZERO;
  compile(p_nd_while->nd_p_while_statement_seq);
  g_code[g_ip].i_opcode = OP_JUMP;
  g_code[g_ip++].i_jump_addr = top_of_loop_addr;
  g_code[backpatch].i_jump_addr = g_ip;
  compile_create_label_name("WHILE_L1", jump_label_name);
  compile_add_jump_label(jump_label_name, g_ip);
  g_n_labels += 1;
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

static void compile_ND_TASK_DECLARATION(PARSE_NODE *p_tree)
{
  LABEL *p_task_label = compile_lookup_label(p_tree->nd_task_name);
  if (NULL == p_task_label)
  {
    p_task_label = compile_add_task_label(p_tree->nd_task_name, g_ip);
    g_n_labels += 1;
  }
  else
  {
    for (BACKPATCH *p_backpatch = p_task_label->lbl_p_backpatch_list;
         NULL != p_backpatch;
         p_backpatch = p_backpatch->bp_p_next
      )
    {
      g_code[p_backpatch->bp_addr].i_jump_addr = g_ip;
    }
  }
  compile_add_counted_string_to_header(p_tree->nd_task_name);
  compile_add_u32_to_header(g_ip);
  compile(p_tree->nd_p_task_body);
  g_code[g_ip++].i_opcode = OP_END_TASK;  // Every task has an implicit 'stop' at the end.
}

void compile_ND_MODULE_DECLARATION(PARSE_NODE *p_tree)
{
  compile(p_tree->nd_p_init_statements);
  g_code[g_ip++].i_opcode = OP_END_TASK;  // Implied 'stop' at end of module initialization.
  for (LISTITEM *p_task_declaration = p_tree->nd_p_task_decl_list;
       NULL != p_task_declaration;
       p_task_declaration = p_task_declaration->l_p_next
    )
  {
    compile(p_task_declaration->l_parse_node);
  }
}

void compile_check_for_undefined_tasks(void)
{
  bool undefined_tasks_found = false;
  for (uint32_t i = 0; i < HTABLE_SIZE; ++i)
  {
    if (NULL != g_hash_table[i])
    {
      for (LABEL *p_label = g_hash_table[i];
           NULL != p_label;
           p_label = p_label->lbl_p_next
        )
      {
        if (NULL != p_label->lbl_p_backpatch_list)
        {
          fprintf(stderr, "Undefined task: %s\n", p_label->lbl_name);
          undefined_tasks_found = true;
        }
      }
    }
  }
  if (undefined_tasks_found)
  {
    exit(0);
  }
}

void compile(PARSE_NODE *p_tree)
{
  if (NULL != p_tree)
  {
    switch (p_tree->nd_type)
    {
      case ND_MODULE_DECLARATION:
        compile_ND_MODULE_DECLARATION(p_tree);
        compile_check_for_undefined_tasks();
        break;
      case ND_TASK_DECLARATION:
        compile_ND_TASK_DECLARATION(p_tree);
        break;
      case ND_STATEMENT_SEQUENCE:
        compile_ND_STATEMENT_SEQUENCE(p_tree);
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
        compile_ND_PRINT_INT(p_tree);
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
