#pragma once

typedef struct TASK TASK;
typedef struct MODULE MODULE;
struct MODULE
{
  char *mod_filename;
  HEADER *mod_p_header;
  INSTRUCTION *mod_p_code;
  TASK *mod_p_init_task;
};

MODULE *module_read(FILE *fin);
void module_free(MODULE *p_module);
