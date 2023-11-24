#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <util.h>

#include "binary-header.h"

// THEORY OF OPERATION:
//
// Header data is read from/written to g_header[] using the simple functions below.  Byte ordering same as CPU.
// "add" means to place data starting at g_idx_header and update the index to point to the next "available" byte after
// the data.
//
// POGO BINARY HEADER FORMAT:
//
// header_size (in bytes) : u32
// n_labels               : u32
// code_size (in bytes)   : u32
// module_name : counted_string
// label_list  : struct
//              {
//                lbl_name : counted_string
//                lbl_type : u8 (0 == jump label, !0 == task label)
//                lbl_addr : u32 // relative to 0th instruction of code.
//              } [n_labels]

#define HEADER_SIZE_IDX 0
#define HEADER_N_LABELS_IDX (sizeof(uint32_t))
#define HEADER_CODE_SIZE_IDX (HEADER_N_LABELS_IDX + sizeof(uint32_t))
#define HEADER_MODULE_NAME_IDX (HEADER_CODE_SIZE_IDX + sizeof(uint32_t))
#define MAX_HEADER_SIZE 4096  // bytes

static uint8_t g_header[MAX_HEADER_SIZE];
static uint32_t g_idx_header = 0;

void bhdr_add_u32_to_header(uint32_t u32)
{
  memcpy(&g_header[g_idx_header], &u32, sizeof(u32));
  g_idx_header += sizeof(u32);
}

static void bhdr_add_u8_to_header(uint8_t u8)
{
  memcpy(&g_header[g_idx_header], &u8, sizeof(u8));
  g_idx_header += sizeof(u8);
}

// Format of counted string: [u32 (bytes)][byte0][byte1]...
static void bhdr_add_counted_string_to_header(char *str)
{
  uint32_t n_bytes = strlen(str);
  bhdr_add_u32_to_header(n_bytes);
  memcpy(&g_header[g_idx_header], str, n_bytes);
  g_idx_header += n_bytes;
}

static uint32_t bhdr_read_header_size(FILE *fin)
{
  uint32_t result;
  if (1 != fread(&result, sizeof(result), 1, fin))
  {
    fprintf(stderr, "Can't read size.\n");
    exit(0);
  }
  return result;
}

static uint32_t bhdr_get_u32(uint32_t ofs)
{
  uint32_t result = *((uint32_t *) (g_header + ofs));
  return result;
}

static uint8_t bhdr_get_u8(uint32_t ofs)
{
  uint8_t result = *((uint8_t *) (g_header + ofs));
  return result;
}

static uint32_t bhdr_get_label_count(void)
{
  uint32_t result = bhdr_get_u32(HEADER_N_LABELS_IDX);
  return result;
}

static uint32_t bhdr_get_code_size_in_bytes(void)
{
  uint32_t result = bhdr_get_u32(HEADER_CODE_SIZE_IDX);
  return result;
}

static uint32_t bhdr_get_counted_string(uint32_t ofs, char *cstr)
{
  uint32_t string_size = bhdr_get_u32(ofs);
  zero_mem(cstr, string_size + 1);  // + 1 for obvious reasons if you've used C for very long.
  memcpy(cstr, g_header + ofs + sizeof(uint32_t), string_size);
  return string_size;
}

static uint32_t bhdr_get_counted_string_size(uint32_t ofs)
{
  uint32_t n_chars = bhdr_get_u32(ofs);
  return n_chars;
}

static uint32_t bhdr_raw_read(FILE *fin, uint32_t n_header_bytes)
{
  uint32_t n_bytes_read = fread(g_header, sizeof(uint8_t), n_header_bytes, fin);
  return n_bytes_read;
}

static uint32_t bhdr_raw_write(FILE *fout)
{
  uint32_t n_bytes_written = fwrite(g_header, sizeof(uint8_t), g_idx_header, fout);
  return n_bytes_written;
}

static void bhdr_init(void)
{
  zero_mem(g_header, MAX_HEADER_SIZE);
  g_idx_header = 0;
}

static uint32_t bhdr_get_label_list_ofs(void)
{
  uint32_t n_chars = bhdr_get_counted_string_size(HEADER_MODULE_NAME_IDX);
  uint32_t ofs = HEADER_MODULE_NAME_IDX + sizeof(uint32_t) + n_chars;
  return ofs;
}

HEADER *bhdr_read(FILE *fin)
{
  uint32_t n_header_bytes;
  uint32_t n_labels;
  uint32_t offset;
  uint32_t n_chars;
  HEADER *result = NULL;
  bhdr_init();
  n_header_bytes = bhdr_read_header_size(fin);
  rewind(fin);
  if (bhdr_raw_read(fin, n_header_bytes) == n_header_bytes)
  {
    n_labels = bhdr_get_label_count();
    if (NULL != (result = malloc(sizeof(HEADER) + n_labels*sizeof(HEADER_LABEL))))
    {
      result->hdr_size_bytes = n_header_bytes;
      result->hdr_n_labels = n_labels;
      result->hdr_code_size_bytes = bhdr_get_code_size_in_bytes();
      bhdr_get_counted_string(HEADER_MODULE_NAME_IDX, result->hdr_module_name);
      offset = bhdr_get_label_list_ofs();
      for (uint32_t i = 0; i < n_labels; ++i)
      {
        n_chars = bhdr_get_counted_string(offset, result->hdr_labels[i].hlbl_name);
        offset += n_chars + sizeof(uint32_t);
        result->hdr_labels[i].hlbl_type = bhdr_get_u8(offset);
        offset += sizeof(uint8_t);
        result->hdr_labels[i].hlbl_addr = bhdr_get_u32(offset);
        offset += sizeof(uint32_t);
      }
    }
  }
  return result;
}

uint32_t bhdr_write(FILE *fout, HEADER *p_header)
{
  uint32_t result = 0;
  bhdr_init();
  bhdr_add_u32_to_header(p_header->hdr_size_bytes);
  bhdr_add_u32_to_header(p_header->hdr_n_labels);
  bhdr_add_u32_to_header(p_header->hdr_code_size_bytes);
  bhdr_add_counted_string_to_header(p_header->hdr_module_name);
  for (uint32_t i = 0; i < p_header->hdr_n_labels; ++i)
  {
    bhdr_add_counted_string_to_header(p_header->hdr_labels[i].hlbl_name);
    bhdr_add_u8_to_header(p_header->hdr_labels[i].hlbl_type);
    bhdr_add_u32_to_header(p_header->hdr_labels[i].hlbl_addr);
  }
  result = bhdr_raw_write(fout);
  return result;
}
