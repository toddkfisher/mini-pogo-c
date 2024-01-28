#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <util.h>

#include "instruction.h"
#include "binary-header.h"
#include "exec.h"
#include "module.h"

#include <enum-str.h>
char *g_opcode_names[] =
{
#include "opcode-enums.txt"
};

static void disasm_print_label_from_addr(uint32_t addr,
                                         HEADER *p_header)
{
  for (uint32_t i = 0; i < p_header->hdr_n_labels; ++i)
  {
    if (addr == p_header->hdr_labels[i].hlbl_addr)
      printf("%s ", p_header->hdr_labels[i].hlbl_name);
  }
}

static void disasm_print_instruction(INSTRUCTION *p_instruct,
                                     uint32_t ip,
                                     uint32_t indent,
                                     HEADER *p_header)
{
  //printf("%04x", ip);
  for (uint32_t i = 0; i < indent; ++i)
    printf(" ");
  printf("%-20s", g_opcode_names[p_instruct->i_opcode]);
  switch (p_instruct->i_opcode)
  {
    case OP_PUSH_CONST_INT:
      printf("%d ", p_instruct->i_const_int);
      break;
    case OP_POP_INT:
    case OP_PUSH_VAR:
      printf("%c ", p_instruct->i_var_name);
      break;
    case OP_JUMP:
    case OP_JUMP_IF_ZERO:
    case OP_JUMP_IF_NONZERO:
      disasm_print_label_from_addr(p_instruct->i_jump_addr, p_header);
      break;
    case OP_SPAWN:
      disasm_print_label_from_addr(p_instruct->i_task_addr, p_header);
      break;
    case OP_PRINT_CHAR:
      if (isprint(p_instruct->i_char))
        printf("'%c' ", p_instruct->i_char);
      else
        printf("%02x ", (uint32_t) p_instruct->i_char);
      break;
    case OP_BEGIN_SPAWN:
      printf("%d ", p_instruct->i_n_spawn_tasks);
      break;
    default:
      break;
  }
  printf("\n");
}

int main(int argc, char **argv)
{
  if (argc != 2)
    fprintf(stderr, "Usage: mpd file\n");
  else
  {
    FILE *fin;
    MODULE *p_module;
    if (NULL == (fin = fopen(argv[1], "r")))
      fprintf(stderr, "%s : unable to open\n", argv[1]);
    else
    {
      if (NULL != (p_module = module_read(fin)))
      {
        fclose(fin);
        uint32_t n_instructions = p_module->mod_p_header->hdr_code_size_bytes/sizeof(INSTRUCTION);
        for (uint32_t i = 0; i < n_instructions; ++i)
        {
          for(uint32_t j = 0; j < p_module->mod_p_header->hdr_n_labels; ++j)
          {
            if (i == p_module->mod_p_header->hdr_labels[j].hlbl_addr)
            {
              if (p_module->mod_p_header->hdr_labels[j].hlbl_type != 0)
                printf("\n\nTASK ");
              printf("%s:\n", p_module->mod_p_header->hdr_labels[j].hlbl_name);
            }
          }
          disasm_print_instruction(&p_module->mod_p_code[i], i, 6, p_module->mod_p_header);
        }
        module_free(p_module);
      }
    }
  }
  return 0;
}
