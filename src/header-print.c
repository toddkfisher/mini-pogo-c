#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <util.h>

#include "binary-header.h"

int main(int argc, char **argv)
{
  if (2 != argc)
  {
    fprintf(stderr, "Usage: hp infile\n");
  }
  else
  {
    FILE *fin = fopen(argv[1], "r");
    bhdr_init();
    uint32_t n_header_bytes = bhdr_read_header_size(fin);
    printf("Header size = %d bytes\n", n_header_bytes);
    rewind(fin);
    if (bhdr_read(fin, n_header_bytes) != n_header_bytes)
    {
      fprintf(stderr, "Error reading header.\n");
    }
    else
    {
      uint32_t n_labels;
      uint32_t n_chars;
      char name[MAX_STR];
      uint8_t lbl_type;
      uint32_t lbl_addr;
      uint32_t ofs;
      n_labels = bhdr_get_label_count();
      n_chars = bhdr_get_counted_string(HEADER_MODULE_NAME_IDX, name);
      printf("Module name: %s\n", name);
      printf("Number of labels = %d\n", n_labels);
      ofs = bhdr_get_label_list_ofs();
      for (uint32_t i = 0; i < n_labels; ++i)
      {
        n_chars = bhdr_get_counted_string(ofs, name);
        ofs += n_chars + sizeof(uint32_t);
        lbl_type = bhdr_get_u8(ofs);
        ofs += sizeof(uint8_t);
        lbl_addr = bhdr_get_u32(ofs);
        ofs += sizeof(uint32_t);
        printf("%16s (%c) @ %08x\n", name, lbl_type ? 'T' : 'J', lbl_addr);
      }
    }
    fclose(fin);
  }
  return 0;
}
