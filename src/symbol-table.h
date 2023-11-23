#pragma once

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

void stab_hash_init(void);
LABEL *stab_lookup_label(char *label);
LABEL *stab_add_label(char *name,
                      bool addr_set,
                      uint32_t addr,
                      bool is_task);
LABEL *stab_add_task_label(char *task_name, uint32_t task_addr);
LABEL *stab_add_forward_ref_task_label(char *task_name);
LABEL *stab_add_jump_label(char *label_name, uint32_t jump_addr);
void stab_add_backpatch(LABEL *p_label, uint32_t addr);
