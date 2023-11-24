#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <util.h>

#include "binary-header.h"
#include "instruction.h"

int main(int argc, char **argv)
{
  if (2 != argc)
    fprintf(stderr, "Usage: hp infile\n");
  else
  {
    FILE *fin = fopen(argv[1], "r");
    HEADER *p_header;
    if (NULL != (p_header = bhdr_read(fin)))
    {
      printf("Header size = %d bytes\n", p_header->hdr_size_bytes);
      printf("Code size = %d (bytes), %lu (instructions)\n", p_header->hdr_code_size_bytes,
             p_header->hdr_code_size_bytes/sizeof(INSTRUCTION));
      printf("Number of labels = %d\n", p_header->hdr_n_labels);
      for (uint32_t i = 0; i < p_header->hdr_n_labels; ++i)
      {
        printf("%16s (%c) @ %08x\n",
               p_header->hdr_labels[i].hlbl_name,
               p_header->hdr_labels[i].hlbl_type != 0 ? 'T' : 'J',
               p_header->hdr_labels[i].hlbl_addr);
      }
      fclose(fin);
      free(p_header);
    }
  }
  return 0;
}
