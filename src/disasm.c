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

#include <enum-str.h>
char *g_opcode_names[] =
{
#include "opcode-enums.txt"
};

static INSTRUCTION g_code[MAX_CODE_SIZE];

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
    HEADER *p_header;
    FILE *fin;
    if (NULL == (fin = fopen(argv[1], "r")))
      fprintf(stderr, "%s : unable to open\n", argv[1]);
    else
    {
      if (NULL != (p_header = bhdr_read(fin)))
      {
        // fin should be positioned at the first code instruction
        if (p_header->hdr_code_size_bytes != fread(g_code, 1, p_header->hdr_code_size_bytes, fin))
          fprintf(stderr, "Error reading code.\n");
        else
        {
          uint32_t n_instructions = p_header->hdr_code_size_bytes/sizeof(INSTRUCTION);
          for (uint32_t i = 0; i < n_instructions; ++i)
          {
            for (uint32_t j = 0; j < p_header->hdr_n_labels; ++j)
            {
              if (i == p_header->hdr_labels[j].hlbl_addr)
              {
                if (p_header->hdr_labels[j].hlbl_type != 0)
                  printf("\n\nTASK ");
                printf("%s:\n", p_header->hdr_labels[j].hlbl_name);
              }
            }
            disasm_print_instruction(&g_code[i], i, 6, p_header);
          }
        }
      }
    }
  }
  return 0;
}
