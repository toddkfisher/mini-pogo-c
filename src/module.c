#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <util.h>


#include "instruction.h"
#include "binary-header.h"
#include "exec.h"
#include "module.h"


MODULE *module_read(FILE *fin)
{
  MODULE *result = NULL;
  result = malloc(sizeof(MODULE));
  result->mod_p_header = bhdr_read(fin);
  result->mod_p_init_task = NULL;
  if (result->mod_p_header)
  {
    fseek(fin, result->mod_p_header->hdr_size_bytes, SEEK_SET);
    result->mod_p_code = malloc(result->mod_p_header->hdr_code_size_bytes);
    if (result->mod_p_header->hdr_code_size_bytes !=
        fread(result->mod_p_code, 1, result->mod_p_header->hdr_code_size_bytes, fin))
    {
      free(result->mod_p_code);
      free(result->mod_p_header);
      free(result);
      result = NULL;
      fprintf(stderr, "Unable to read code.\n");
    }
  }
  else
  {
    free(result);
    result = NULL;
    fprintf(stderr, "Unable to read module.\n");
  }
  return result;
}


void module_free(MODULE *p_module)
{
  free(p_module->mod_p_code);
  free(p_module->mod_p_header);
  if (p_module->mod_p_init_task)
    free(p_module->mod_p_init_task);
  free(p_module);
}
