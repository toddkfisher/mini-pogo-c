#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <util.h>
//------------------------------------------------------------------------------
#include "parse.h"
#include "lex.h"
#include "instruction.h"
#include "binary-header.h"
#include "compile.h"
#include "symbol-table.h"
#include "string-table.h"
//------------------------------------------------------------------------------
static uint32_t g_n_labels = 0;
static INSTRUCTION g_code[MAX_CODE_SIZE];
static uint32_t g_ip = 0;
static char g_module_name[MAX_STR];
//------------------------------------------------------------------------------
extern uint32_t g_n_strings;
extern STRING_CONST *g_hash_strings[STRING_HTABLE_SIZE];
extern LABEL *g_hash_labels[SYMBOL_HTABLE_SIZE];
extern uint32_t g_n_labels;
static uint32_t g_label_suffix_number = 0;
//------------------------------------------------------------------------------
static void compile_create_label_name(char *prefix, char *name_dest)
{
  sprintf(name_dest, "<%s_%u>", prefix, g_label_suffix_number++);
}
//------------------------------------------------------------------------------
void compile_OP_END_TASK(void)
{
  g_code[g_ip++].i_opcode = OP_END_TASK;
}
//------------------------------------------------------------------------------
void compile_binary_op(uint8_t opcode, PARSE_NODE *p_tree)
{
  compile(p_tree->nd_p_left_expr);
  compile(p_tree->nd_p_right_expr);
  g_code[g_ip++].i_opcode = opcode;
}
//------------------------------------------------------------------------------
void compile_OP_BEGIN_SPAWN(void)
{
  g_code[g_ip++].i_opcode = OP_BEGIN_SPAWN;
}
//------------------------------------------------------------------------------
void compile_OP_SPAWN(uint32_t task_addr)
{
  g_code[g_ip].i_opcode = OP_SPAWN;
  g_code[g_ip++].i_task_addr = task_addr;
}
//------------------------------------------------------------------------------
void compile_OP_PRINT_INT(void)
{
  g_code[g_ip++].i_opcode = OP_PRINT_INT;
}
//------------------------------------------------------------------------------
void compile_OP_PUSH_VAR(char var_name)
{
  g_code[g_ip].i_var_name = var_name;
  g_code[g_ip++].i_opcode = OP_PUSH_VAR;
}
//------------------------------------------------------------------------------
void compile_OP_JOIN(void)
{
  g_code[g_ip++].i_opcode = OP_JOIN;
}
//------------------------------------------------------------------------------
void compile_OP_WAIT_JUMP(void)
{
  g_code[g_ip++].i_opcode = OP_WAIT_JUMP;
}
//------------------------------------------------------------------------------
void compile_ND_SPAWN(PARSE_NODE *p_nd_spawn)
{
  uint32_t n_spawn_tasks = 0;
  uint32_t begin_spawn_addr = g_ip;
  // "spawn t0; t1; t2; end"
  // compiles to:
  //     BEGIN_SPAWN 3
  //     SPAWN <addr of t0>
  //     SPAWN <addr of t1>
  //     SPAWN <addr of t2>
  //     JOIN
  //
  // 1) If  t_k isn't in the  symbol table, then we  add it and g_ip  for future
  //    backpatching.
  //
  // 2) If t_k is in the symbol table and it address is set (lbl_addr_set), then
  //    we use the address and no backpatching is necessary.
  //
  // 3) If t_k is in the symbol table and its address is not set (lbl_addr_set),
  //    then we add it and g_ip++ for future backpatching.
  compile_OP_BEGIN_SPAWN();
  for (LISTITEM *p_task_name = p_nd_spawn->nd_p_task_names;
       p_task_name;
       p_task_name = p_task_name->l_p_next)
  {
    LABEL *p_label = symtab_lookup_label(p_task_name->l_name);
    uint32_t task_addr = 0;
    if (!p_label)
    {
      // Case 1.
      p_label = symtab_add_forward_ref_task_label(p_task_name->l_name);
      g_n_labels += 1;
      symtab_add_backpatch(p_label, g_ip);
    }
    else
    {
      if (p_label->lbl_addr_set)
        // Case 2.
        task_addr = p_label->lbl_addr;
      else
        // Case 3.
        symtab_add_backpatch(p_label, g_ip);
    }
    n_spawn_tasks += 1;
    compile_OP_SPAWN(task_addr);
  }
  g_code[begin_spawn_addr].i_n_spawn_tasks = n_spawn_tasks;
  //  spawn t0;t1;t2;
  //  wait 1000*2
  //  timeout
  //    print "join timed out";
  //  else
  //    print "join successful"
  //  end
  //
  // Compiles to:
  //
  //   BEGIN_SPAWN 3
  //   SPAWN t0
  //   SPAWN t1
  //   SPAWN t2
  //   PUSH_INT_CONST 1000
  //   PUSH_INT_CONST 2
  //   MULTIPLY
  //   WAIT_JUMP L0  <- else_jump_addr (L0 gets poked here)
  //   ATOMIC_PRINT "join timed out"
  //   JUMP L1 <- jump_over_else_addr (L1 gets poked here)
  // L0:
  //   ATOMIC_PRINT "join successful"
  // L1:
  if (p_nd_spawn->nd_p_millisec_expr)  // nd_p_millisec_expr != NULL if we have a timeout.
  {
    char jump_label_name[MAX_STR];
    uint32_t else_jump_addr;
    uint32_t jump_over_else_addr;
    // Compile in a similar fashion to 'if'/'then'/'else'
    compile(p_nd_spawn->nd_p_millisec_expr);
    else_jump_addr = g_ip;
    compile_OP_WAIT_JUMP();
    if (p_nd_spawn->nd_p_statement_seq_if_timed_out)
      compile(p_nd_spawn->nd_p_statement_seq_if_timed_out);
    jump_over_else_addr = g_ip;
    g_code[g_ip].i_opcode = OP_JUMP;
    g_ip += 1;
    if (p_nd_spawn->nd_p_statement_seq_if_not_timed_out)
    {
      compile_create_label_name("ELSE", jump_label_name);
      symtab_add_jump_label(jump_label_name, g_ip);
      g_n_labels += 1;
      g_code[else_jump_addr].i_jump_addr = g_ip;
      compile(p_nd_spawn->nd_p_statement_seq_if_not_timed_out);
    }
    compile_create_label_name("END-SPAWN", jump_label_name);
    symtab_add_jump_label(jump_label_name, g_ip);
    g_n_labels += 1;
    g_code[jump_over_else_addr].i_jump_addr = g_ip;
  }
  else  // No timeout.
  {
    compile_OP_JOIN();
  }
}
//------------------------------------------------------------------------------
static void compile_ND_AND(PARSE_NODE *p_tree)
{
  char jump_label_name[MAX_STR];
  uint32_t backpatch;
  // left 'and' right
  // compiles to:
  //    compile(left)
  //    OP_TEST_AND_JUMP_IF_ZERO L0
  //    comple(right)
  //  L0:
  compile(p_tree->nd_p_left_expr);
  backpatch = g_ip;
  g_code[g_ip].i_opcode = OP_TEST_AND_JUMP_IF_ZERO;
  g_code[g_ip++].i_jump_addr = 0;
  compile(p_tree->nd_p_right_expr);
  compile_create_label_name("AND-SHORT-CIRCUIT", jump_label_name);
  symtab_add_jump_label(jump_label_name, g_ip);
  g_n_labels += 1;
  g_code[backpatch].i_jump_addr = g_ip;
}
//------------------------------------------------------------------------------
static void compile_ND_OR(PARSE_NODE *p_tree)
{
  char jump_label_name[MAX_STR];
  uint32_t backpatch;
  // left 'or' right
  // compiles to:
  //    compile(left)
  //    OP_TEST_AND_JUMP_IF_NONZERO L0
  //    comple(right)
  //  L0:
  compile(p_tree->nd_p_left_expr);
  backpatch = g_ip;
  g_code[g_ip].i_opcode = OP_TEST_AND_JUMP_IF_NONZERO;
  g_code[g_ip++].i_jump_addr = 0;
  compile(p_tree->nd_p_right_expr);
  compile_create_label_name("OR-SHORT-CIRCUIT", jump_label_name);
  symtab_add_jump_label(jump_label_name, g_ip);
  g_n_labels += 1;
  g_code[backpatch].i_jump_addr = g_ip;
}
//------------------------------------------------------------------------------
static void compile_ND_IF(PARSE_NODE *p_nd_if)
{
  uint32_t condition_false_jump_addr;
  uint32_t jump_to_end_if_addr;
  char jump_label_name[MAX_STR];
  // "if p then ss0 else ss1 end"
  // compiles to:
  //     compile(p)
  //     JUMP_IF_ZERO <ELSE> ; <- condition_false_jump_addr location
  //     compile(ss0)        ; could be empty
  //     JUMP END-IF         ; <- jump_to_end_if_addr location
  //   ELSE:                 ; address for jump instruction at condition_false_jump_addr
  //     compile(ss1)        ; could be empty
  //   END-IF:               ; address for jump instruction at jump_to_end_if_addr
  // "if p then ss0 end"
  // compiles to:
  //     compile(p)
  //     JUMP_IF_ZERO L0     ; <- condition_false_jump_addr/jump_to_end_if_addr location.
  //     compile(ss0)
  //   L0:
  compile(p_nd_if->nd_p_if_test_expr);
  condition_false_jump_addr = g_ip;
  g_code[g_ip].i_opcode = OP_JUMP_IF_ZERO;
  g_code[g_ip++].i_jump_addr = 0;
  compile(p_nd_if->nd_p_true_branch_statement_seq);
  if (p_nd_if->nd_p_false_branch_statement_seq)
  {
    jump_to_end_if_addr = g_ip;
    g_code[g_ip].i_opcode = OP_JUMP;
    g_code[g_ip++].i_jump_addr = 0;
    compile_create_label_name("ELSE", jump_label_name);
    symtab_add_jump_label(jump_label_name, g_ip);
    g_n_labels += 1;
    g_code[condition_false_jump_addr].i_jump_addr = g_ip;
    compile(p_nd_if->nd_p_false_branch_statement_seq);
  }
  else
    jump_to_end_if_addr = condition_false_jump_addr;
  compile_create_label_name("END-IF", jump_label_name);
  symtab_add_jump_label(jump_label_name, g_ip);
  g_n_labels += 1;
  g_code[jump_to_end_if_addr].i_jump_addr = g_ip;
}
//------------------------------------------------------------------------------
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
  compile_create_label_name("WHILE", jump_label_name);
  symtab_add_jump_label(jump_label_name, g_ip);
  g_n_labels += 1;
  compile(p_nd_while->nd_p_while_test_expr);
  backpatch = g_ip;
  g_code[g_ip++].i_opcode = OP_JUMP_IF_ZERO;
  compile(p_nd_while->nd_p_while_statement_seq);
  g_code[g_ip].i_opcode = OP_JUMP;
  g_code[g_ip++].i_jump_addr = top_of_loop_addr;
  g_code[backpatch].i_jump_addr = g_ip;
  compile_create_label_name("END-WHILE", jump_label_name);
  symtab_add_jump_label(jump_label_name, g_ip);
  g_n_labels += 1;
}
//------------------------------------------------------------------------------
static void compile_OP_POP_INT(char var_name)
{
  g_code[g_ip].i_opcode = OP_POP_INT;
  g_code[g_ip++].i_var_name = var_name;
}
//------------------------------------------------------------------------------
static void compile_OP_PRINT_CHAR(char ch)
{
  g_code[g_ip].i_opcode = OP_PRINT_CHAR;
  g_code[g_ip++].i_char = ch;
}
//------------------------------------------------------------------------------
static void compile_OP_PUSH_CONST_INT(int32_t n)
{
  g_code[g_ip].i_opcode = OP_PUSH_CONST_INT;
  g_code[g_ip++].i_const_int = n;
}
//------------------------------------------------------------------------------
static void compile_ND_PRINT_INT(PARSE_NODE *p_tree)
{
  compile(p_tree->nd_p_expr);
  g_code[g_ip++].i_opcode = OP_PRINT_INT;
}
//------------------------------------------------------------------------------
static void compile_ND_SLEEP(PARSE_NODE *p_tree)
{
  compile(p_tree->nd_p_expr);
  g_code[g_ip++].i_opcode = OP_SLEEP;
}
//------------------------------------------------------------------------------
static void compile_ND_NEGATE(PARSE_NODE *p_tree)
{
  compile(p_tree->nd_p_expr);
  g_code[g_ip++].i_opcode = OP_NEGATE;
}
//------------------------------------------------------------------------------
static void compile_ND_ASSIGN(PARSE_NODE *p_tree)
{
  compile(p_tree->nd_p_assign_expr);
  compile_OP_POP_INT(p_tree->nd_var_name);
}
//------------------------------------------------------------------------------
static void compile_ND_STATEMENT_SEQUENCE(PARSE_NODE *p_tree)
{
  for (LISTITEM *p_statement = p_tree->nd_p_statement_seq;
       p_statement;
       p_statement = p_statement->l_p_next)
  {
    compile(p_statement->l_parse_node);
  }
}
//------------------------------------------------------------------------------
static void compile_ND_TASK_DECLARATION(PARSE_NODE *p_tree)
{
  LABEL *p_task_label = symtab_lookup_label(p_tree->nd_task_name);
  if (!p_task_label)
  {
    p_task_label = symtab_add_task_label(p_tree->nd_task_name, g_ip);
    g_n_labels += 1;
  }
  else
  {
    BACKPATCH *p_prev_backpatch = NULL;
    p_task_label->lbl_addr = g_ip;
    p_task_label->lbl_addr_set = true;
    for (BACKPATCH *p_backpatch = p_task_label->lbl_p_backpatch_list;
         p_backpatch;
         p_backpatch = p_backpatch->bp_p_next)
    {
      g_code[p_backpatch->bp_addr].i_jump_addr = g_ip;
      if (p_prev_backpatch)
        free(p_prev_backpatch);
      p_prev_backpatch = p_backpatch;
    }
    if (p_prev_backpatch)
      free(p_prev_backpatch);
    p_task_label->lbl_p_backpatch_list = NULL;
  }
  compile(p_tree->nd_p_task_body);
  compile_OP_END_TASK();  // Every task has an implicit 'stop' at the end.
}
//------------------------------------------------------------------------------
void compile_ND_MODULE_DECLARATION(PARSE_NODE *p_tree)
{
  compile(p_tree->nd_p_init_statements);
  g_code[g_ip++].i_opcode = OP_END_TASK;  // Implied  'stop'  at end  of  module
                                          // initialization.
  for (LISTITEM *p_task_declaration = p_tree->nd_p_task_decl_list;
       p_task_declaration;
       p_task_declaration = p_task_declaration->l_p_next)
  {
    compile(p_task_declaration->l_parse_node);
  }
}
//------------------------------------------------------------------------------
void compile_check_for_undefined_tasks(void)
{
  bool undefined_tasks_found = false;
  for (uint32_t i = 0; i < SYMBOL_HTABLE_SIZE; ++i)
  {
    if (g_hash_labels[i])
    {
      for (LABEL *p_label = g_hash_labels[i];
           p_label;
           p_label = p_label->lbl_p_next)
      {
        if (p_label->lbl_p_backpatch_list)
        {
          fprintf(stderr, "Undefined task: %s\n", p_label->lbl_name);
          undefined_tasks_found = true;
        }
      }
    }
  }
  if (undefined_tasks_found)
    error_exit(0);
}
//------------------------------------------------------------------------------
void compile_init(void)
{
  symtab_hash_init();
  strtab_init();
  g_ip = 0;
}
//------------------------------------------------------------------------------
uint32_t compile_write_header(FILE *fout)
{
  uint32_t idx_label;
  uint32_t n_bytes_header = 4*sizeof(uint32_t);
  HEADER *p_header = NULL;
  uint32_t result = 0;
  // NOTE:  memory overflow  not  checked because  it  increases the  complexity
  //       considerably.  This is only a prototype/proof-of-concept so I'm going
  //       to pretend that mem ovfl doesn't exist.
  p_header = malloc(sizeof(HEADER));
  strcpy(p_header->hdr_module_name, g_module_name);
  n_bytes_header += sizeof(uint32_t) + strlen(g_module_name);
  p_header->hdr_code_size_bytes = g_ip*sizeof(INSTRUCTION);
  idx_label = 0;
  p_header->hdr_p_label_list = malloc(g_n_labels*sizeof(HEADER_LABEL));
  for (uint32_t i = 0; i < SYMBOL_HTABLE_SIZE; ++i)
  {
    for (LABEL *p_label = g_hash_labels[i];
         p_label;
         p_label = p_label->lbl_p_next)
    {
      strcpy(p_header->hdr_p_label_list[idx_label].hlbl_name, p_label->lbl_name);
      // counted string:char count         chars ....
      n_bytes_header += sizeof(uint32_t) + strlen(p_label->lbl_name);
      p_header->hdr_p_label_list[idx_label].hlbl_type = (uint8_t) p_label->lbl_is_task;
      n_bytes_header += sizeof(uint8_t);
      p_header->hdr_p_label_list[idx_label].hlbl_addr = p_label->lbl_addr;
      n_bytes_header += sizeof(uint32_t);
      idx_label += 1;
    }
  }
  p_header->hdr_n_labels = g_n_labels;
  p_header->hdr_p_string_list = malloc(sizeof(char *)*g_n_strings);
  if (p_header->hdr_n_strings = g_n_strings)
  {
    for (uint32_t i = 0; i < STRING_HTABLE_SIZE; ++i)
    {
      if (g_hash_strings[i])
      {
        for (STRING_CONST *p_str = g_hash_strings[i];
             p_str;
             p_str = p_str->s_p_next)
        {
          p_header->hdr_p_string_list[p_str->s_table_index] = strdup(p_str->s_string);
          // counted string:char count         chars ....
          n_bytes_header += sizeof(uint32_t) + strlen(p_str->s_string);
        }
      }
    }
  }
  p_header->hdr_size_bytes = n_bytes_header;
  result = bhdr_write(fout, p_header);
  return result;
}
//------------------------------------------------------------------------------
uint32_t compile_write_code(FILE *fout)
{
  uint32_t n_instructions_written = fwrite(g_code, sizeof(INSTRUCTION), g_ip, fout);
  return n_instructions_written;
}
//------------------------------------------------------------------------------
void compile_ND_PRINT_STRING(PARSE_NODE *p_tree)
{
  STRING_CONST *p_str = strtab_add_string(p_tree->nd_string_const);
  g_code[g_ip].i_opcode = OP_PRINT_STRING;
  g_code[g_ip].i_string_idx = p_str->s_table_index;
  g_ip += 1;
}
//------------------------------------------------------------------------------
void compile_OP_BEGIN_ATOMIC_PRINT(void)
{
  g_code[g_ip++].i_opcode = OP_BEGIN_ATOMIC_PRINT;
}
//------------------------------------------------------------------------------
void compile_OP_END_ATOMIC_PRINT(void)
{
  g_code[g_ip++].i_opcode = OP_END_ATOMIC_PRINT;
}
//------------------------------------------------------------------------------
void compile_ND_ATOMIC_PRINT(PARSE_NODE *p_tree)
{
  compile_OP_BEGIN_ATOMIC_PRINT();
  for (LISTITEM *p_print_item = p_tree->nd_p_print_items;
       p_print_item;
       p_print_item = p_print_item->l_p_next)
  {
    compile(p_print_item->l_parse_node);
  }
  compile_OP_END_ATOMIC_PRINT();
}
//------------------------------------------------------------------------------
void compile(PARSE_NODE *p_tree)
{
  if (p_tree)
  {
    switch (p_tree->nd_type)
    {
      case ND_MODULE_DECLARATION:
        compile_ND_MODULE_DECLARATION(p_tree);
        compile_check_for_undefined_tasks();
        strcpy(g_module_name, p_tree->nd_module_name);
        break;
      case ND_TASK_DECLARATION:
        compile_ND_TASK_DECLARATION(p_tree);
        break;
      case ND_STATEMENT_SEQUENCE:
        compile_ND_STATEMENT_SEQUENCE(p_tree);
        break;
      case ND_ASSIGN:
        compile_ND_ASSIGN(p_tree);
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
      case ND_SLEEP:
        compile_ND_SLEEP(p_tree);
        break;
      case ND_SPAWN_JOIN:
        compile_ND_SPAWN(p_tree);
        break;
      case ND_STOP:
        compile_OP_END_TASK();
        break;
      case ND_OR:
        compile_ND_OR(p_tree);
        break;
      case ND_AND:
        compile_ND_AND(p_tree);
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
      case ND_REMAINDER:
        compile_binary_op(OP_REMAINDER, p_tree);
        break;
      case ND_VARIABLE:
        compile_OP_PUSH_VAR(p_tree->nd_var_name);
        break;
      case ND_NUMBER:
        compile_OP_PUSH_CONST_INT(p_tree->nd_number);
        break;
      case ND_PRINT_STRING:
        compile_ND_PRINT_STRING(p_tree);
        break;
      case ND_ATOMIC_PRINT:
        compile_ND_ATOMIC_PRINT(p_tree);
        break;
      default:
        break;
    }
  }
}
