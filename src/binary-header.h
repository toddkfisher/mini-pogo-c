#pragma once

typedef struct HEADER_LABEL
{
  char hlbl_name[MAX_STR];
  uint8_t hlbl_type;
  uint32_t hlbl_addr;
} HEADER_LABEL;

typedef struct HEADER
{
  uint32_t hdr_size_bytes;
  uint32_t hdr_n_labels;
  uint32_t hdr_code_size_bytes;
  char hdr_module_name[MAX_STR];
  HEADER_LABEL hdr_labels[];
} HEADER;

uint32_t bhdr_write(FILE *fout, HEADER *p_header);
HEADER *bhdr_read(FILE *fin);
