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
    if (p_header = bhdr_read(fin))
    {
      bhdr_print_struct(p_header);
      fclose(fin);
      free(p_header);
    }
  }
  return 0;
}
