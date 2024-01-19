#pragma once

typedef struct MODULE
{
  char *mod_filename;
  HEADER *mod_p_header;
  INSTRUCTION *mod_p_code;
} MODULE;

MODULE *module_read(FILE *fin);
void module_free(MODULE *p_module);
