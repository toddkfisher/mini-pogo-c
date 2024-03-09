#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <util.h>
//------------------------------------------------------------------------------
#include "symbol-table.h"
//------------------------------------------------------------------------------
LABEL *g_hash_labels[SYMBOL_HTABLE_SIZE];
uint32_t g_n_labels;
//------------------------------------------------------------------------------
static uint32_t symtab_hash(char *s)
{
  uint32_t char_sum = 0;
  uint32_t result;
  while (*s)
    char_sum += *s++;
  result = char_sum % SYMBOL_HTABLE_SIZE;
  return result;
}
//------------------------------------------------------------------------------
void symtab_hash_init(void)
{
  g_n_labels = 0;
  for (uint32_t i = 0; i < SYMBOL_HTABLE_SIZE; ++i)
    g_hash_labels[i] = NULL;
}
//------------------------------------------------------------------------------
LABEL *symtab_lookup_label(char *label)
{
  LABEL *result;
  for (result = g_hash_labels[symtab_hash(label)];
       result && !STREQ(label, result->lbl_name);
       result = result->lbl_p_next)
  {
    // Empty loop body.  Everything happens in for loop header.
  }
  return result;
}
//------------------------------------------------------------------------------
LABEL *symtab_add_label(char *name,
                        bool addr_set,
                        uint32_t addr,
                        bool is_task)
{
  uint32_t h = symtab_hash(name);
  LABEL *result = malloc(sizeof(LABEL));
  strcpy(result->lbl_name, name);
  result->lbl_addr_set = false;
  result->lbl_addr = addr;
  result->lbl_is_task = is_task;
  result->lbl_p_backpatch_list = NULL;
  result->lbl_p_next = g_hash_labels[h];
  g_hash_labels[h] = result;
  g_n_labels += 1;
  return result;
}
//------------------------------------------------------------------------------
LABEL *symtab_add_task_label(char *task_name, uint32_t task_addr)
{
  LABEL *result = symtab_add_label(task_name,
                                   /*addr_set*/ true,
                                   task_addr,
                                   /*is_task*/ true);
  return result;
}
//------------------------------------------------------------------------------
LABEL *symtab_add_forward_ref_task_label(char *task_name)
{
  LABEL *result = symtab_add_label(task_name,
                                   /*addr_set*/ false,
                                   /*addr*/ 0,
                                   /*is_task*/ true);
  return result;
}
//------------------------------------------------------------------------------
LABEL *symtab_add_jump_label(char *label_name, uint32_t jump_addr)
{
  LABEL *result = symtab_add_label(label_name,
                                   /*addr_set*/ true,
                                   /* addr */ jump_addr,
                                   /*is_task*/ false);
  return result;
}
//------------------------------------------------------------------------------
void symtab_add_backpatch(LABEL *p_label, uint32_t addr)
{
  BACKPATCH *p_backpatch = malloc(sizeof(BACKPATCH));
  p_backpatch->bp_addr = addr;
  p_backpatch->bp_p_next = p_label->lbl_p_backpatch_list;
  p_label->lbl_p_backpatch_list = p_backpatch;
}
