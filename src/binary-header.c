#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <util.h>
//------------------------------------------------------------------------------
#include "lex.h"
#include "binary-header.h"
#include "instruction.h"
//------------------------------------------------------------------------------
// THEORY OF OPERATION:
//
// Header data is read from/written to g_raw_header[] using the simple functions
// below.  Byte ordering same as CPU.  Functions named "add" place data starting
// at g_idx_header and update the index to point to the next byte after the data
// (u32 or u8) is added.
//
// POGO BINARY HEADER FORMAT (IN FILE) (SEE ALSO struct HEADER).
//
// header_size (in bytes) : u32
// n_labels               : u32
// n_strings              : u32
// code_size (in bytes)   : u32
// module_name            : counted_string
// string_list[n_strings] : counted_string;
// label_list[n_labels]   : struct
//                         {
//                           lbl_name : counted_string
//                           lbl_type : u8 (0 == jump label, !0 == task label)
//                           lbl_addr : u32 // relative to 0th instruction of code.
//                         };
//
// NOTE: THESE ROUTINES ARE NOT RE-ENTRANT. (dynamic module loading
//       won't work)
//------------------------------------------------------------------------------
// Field offsets into file.
#define HEADER_SIZE_IDX 0
#define HEADER_N_LABELS_IDX (sizeof(uint32_t))
#define HEADER_N_STRINGS_IDX (HEADER_N_LABELS_IDX + sizeof(uint32_t))
#define HEADER_CODE_SIZE_IDX (HEADER_N_STRINGS_IDX + sizeof(uint32_t))
#define HEADER_MODULE_NAME_IDX (HEADER_CODE_SIZE_IDX + sizeof(uint32_t))
#define MAX_HEADER_SIZE 32768  // bytes
//------------------------------------------------------------------------------
static uint8_t g_raw_header[MAX_HEADER_SIZE];  // Blob of binary read from or
                                               // written to file.
static uint32_t g_idx_header = 0;
//------------------------------------------------------------------------------
// Place a u32 into header at "current index"
void bhdr_add_u32_to_header(uint32_t u32)
{
  memcpy(&g_raw_header[g_idx_header], &u32, sizeof(u32));
  g_idx_header += sizeof(u32);
}
//------------------------------------------------------------------------------
// Place a u8 into header at "current index"
static void bhdr_add_u8_to_header(uint8_t u8)
{
  memcpy(&g_raw_header[g_idx_header], &u8, sizeof(u8));
  g_idx_header += sizeof(u8);
}
//------------------------------------------------------------------------------
// Take a  C nul-terminated string and  place an equivalent counted  string into
// the header. Format of counted string: [u32 (bytes)][byte0][byte1]...
static void bhdr_add_counted_string_to_header(char *str)
{
  uint32_t n_bytes = strlen(str);
  bhdr_add_u32_to_header(n_bytes);
  memcpy(&g_raw_header[g_idx_header], str, n_bytes);
  g_idx_header += n_bytes;
}
//------------------------------------------------------------------------------
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
//------------------------------------------------------------------------------
// Get 4-byte unsigned at ofs from blob.
static uint32_t bhdr_get_u32(uint32_t ofs)
{
  uint32_t result = *((uint32_t *) (g_raw_header + ofs));
  return result;
}
//------------------------------------------------------------------------------
// Get 1-byte unsigned at ofs from blob.
static uint8_t bhdr_get_u8(uint32_t ofs)
{
  uint8_t result = *((uint8_t *) (g_raw_header + ofs));
  return result;
}
//------------------------------------------------------------------------------
static uint32_t bhdr_get_label_count(void)
{
  uint32_t result = bhdr_get_u32(HEADER_N_LABELS_IDX);
  return result;
}
//------------------------------------------------------------------------------
static uint32_t bhdr_get_string_count(void)
{
  uint32_t result = bhdr_get_u32(HEADER_N_STRINGS_IDX);
  return result;
}
//------------------------------------------------------------------------------
static uint32_t bhdr_get_code_size_in_bytes(void)
{
  uint32_t result = bhdr_get_u32(HEADER_CODE_SIZE_IDX);
  return result;
}
//------------------------------------------------------------------------------
static uint32_t bhdr_get_counted_string(uint32_t ofs, char *p_dest)
{
  uint32_t string_size = bhdr_get_u32(ofs);
  zero_mem(p_dest, string_size + 1);  // + 1 for obvious  reasons if you've used
                                      // C for very long.
  memcpy(p_dest, g_raw_header + ofs + sizeof(uint32_t), string_size);
  return string_size;
}
//------------------------------------------------------------------------------
static uint32_t bhdr_get_counted_string_size(uint32_t ofs)
{
  uint32_t n_chars = bhdr_get_u32(ofs);
  return n_chars;
}
//------------------------------------------------------------------------------
static uint32_t bhdr_raw_read(FILE *fin, uint32_t n_header_bytes)
{
  uint32_t n_bytes_read = fread(g_raw_header, sizeof(uint8_t), n_header_bytes,
                                fin);
  return n_bytes_read;
}
//------------------------------------------------------------------------------
static uint32_t bhdr_raw_write(FILE *fout)
{
  uint32_t n_bytes_written = fwrite(g_raw_header, sizeof(uint8_t), g_idx_header,
                                    fout);
  return n_bytes_written;
}
//------------------------------------------------------------------------------
static void bhdr_init(void)
{
  zero_mem(g_raw_header, MAX_HEADER_SIZE);
  g_idx_header = 0;
}
//------------------------------------------------------------------------------
static uint32_t bhdr_get_string_list_ofs(void)
{
  uint32_t n_chars = bhdr_get_counted_string_size(HEADER_MODULE_NAME_IDX);
  uint32_t ofs = HEADER_MODULE_NAME_IDX + sizeof(uint32_t) + n_chars;
  return ofs;
}
//------------------------------------------------------------------------------
// Allocate new header and arrays contained therein.
// RETURN: NULL if memory overflow.
static HEADER *bhdr_new(uint32_t n_labels, uint32_t n_strings)
{
  HEADER *result = malloc(sizeof(HEADER));
  uint32_t idx_str;
  if (result)
  {
    if (n_strings > 0)
    {
      if (!(result->hdr_p_string_list = malloc(sizeof(char *)*n_strings)))
        goto ERROR_EXIT_0;
      for (idx_str = 0; idx_str < n_strings; ++idx_str)
      {
        if (!(result->hdr_p_string_list[idx_str] = malloc(sizeof(char)*MAX_STR)))
          goto ERROR_EXIT_1;
      }
      if (!(result->hdr_p_label_list = malloc(sizeof(HEADER_LABEL)*n_labels)))
        goto ERROR_EXIT_1;
    }
  }
  return result;
ERROR_EXIT_0:
  free(result);
  return NULL;
ERROR_EXIT_1:
  for (uint32_t idx_free = 0; idx_free < idx_str; ++idx_free)
    free(result->hdr_p_string_list[idx_free]);
  free(result);
  return NULL;
}
//------------------------------------------------------------------------------
HEADER *bhdr_read(FILE *fin)
{
  uint32_t n_header_bytes;
  uint32_t n_labels;
  uint32_t n_strings;
  uint32_t offset;
  uint32_t n_chars;
  HEADER *result = NULL;
  bhdr_init();
  n_header_bytes = bhdr_read_header_size(fin);
  rewind(fin);
  if (bhdr_raw_read(fin, n_header_bytes) == n_header_bytes)
  {
    n_labels = bhdr_get_label_count();
    n_strings = bhdr_get_string_count();
    if (result = bhdr_new(n_labels, n_strings))
    {
      result->hdr_size_bytes = n_header_bytes;
      result->hdr_n_strings = n_strings;
      result->hdr_n_labels = n_labels;
      result->hdr_code_size_bytes = bhdr_get_code_size_in_bytes();
      n_chars = bhdr_get_counted_string(HEADER_MODULE_NAME_IDX, result->hdr_module_name);
      //                                count              chars ...
      offset = HEADER_MODULE_NAME_IDX + sizeof(uint32_t) + n_chars;
      for (uint32_t idx_string = 0; idx_string < n_strings; ++idx_string)
      {
        n_chars = bhdr_get_counted_string(offset, result->hdr_p_string_list[idx_string]);
        //        count              chars...
        offset += sizeof(uint32_t) + n_chars;
      }
      for (uint32_t idx_label = 0; idx_label < n_labels; ++idx_label)
      {
        n_chars = bhdr_get_counted_string(offset, result->hdr_p_label_list[idx_label].hlbl_name);
        offset += n_chars + sizeof(uint32_t);
        result->hdr_p_label_list[idx_label].hlbl_type = bhdr_get_u8(offset);
        offset += sizeof(uint8_t);
        result->hdr_p_label_list[idx_label].hlbl_addr = bhdr_get_u32(offset);
        offset += sizeof(uint32_t);
      }
    }
  }
  return result;
}
//------------------------------------------------------------------------------
void bhdr_print_struct(HEADER *p_header)
{
  printf("      Module name = %s\n", p_header->hdr_module_name);
  printf("      Header size = %d bytes\n", p_header->hdr_size_bytes);
  printf("        Code size = %d (bytes), %lu (instructions)\n", p_header->hdr_code_size_bytes,
         p_header->hdr_code_size_bytes/sizeof(INSTRUCTION));
  printf(" Number of labels = %d\n", p_header->hdr_n_labels);
  printf("Number of strings = %d\n", p_header->hdr_n_strings);
  if (p_header->hdr_n_strings)
  {
    printf("--Begin string list--\n");
    for (uint32_t i = 0; i < p_header->hdr_n_strings; ++i)
    {
      printf("%04d: ", i);
      lex_print_string_escaped(p_header->hdr_p_string_list[i]);
      printf("\n");
    }
    printf("--End string list--\n");
  }
  if (p_header->hdr_n_labels)
  {
    printf("--Begin label list--\n");
    for (uint32_t i = 0; i < p_header->hdr_n_labels; ++i)
    {
      printf("%30s (%c) @ %08x\n",
             p_header->hdr_p_label_list[i].hlbl_name,
             p_header->hdr_p_label_list[i].hlbl_type != 0 ? 'T' : 'J',
             p_header->hdr_p_label_list[i].hlbl_addr);
    }
    printf("--End label list--\n");
  }
}
//------------------------------------------------------------------------------
uint32_t bhdr_write(FILE *fout, HEADER *p_header)
{
  uint32_t result = 0;
  bhdr_init();
  bhdr_add_u32_to_header(p_header->hdr_size_bytes);
  bhdr_add_u32_to_header(p_header->hdr_n_labels);
  bhdr_add_u32_to_header(p_header->hdr_n_strings);
  bhdr_add_u32_to_header(p_header->hdr_code_size_bytes);
  bhdr_add_counted_string_to_header(p_header->hdr_module_name);
  for (uint32_t idx_string = 0; idx_string < p_header->hdr_n_strings; ++idx_string)
    bhdr_add_counted_string_to_header(p_header->hdr_p_string_list[idx_string]);
  for (uint32_t idx_label = 0; idx_label < p_header->hdr_n_labels; ++idx_label)
  {
    bhdr_add_counted_string_to_header(p_header->hdr_p_label_list[idx_label].hlbl_name);
    bhdr_add_u8_to_header(p_header->hdr_p_label_list[idx_label].hlbl_type);
    bhdr_add_u32_to_header(p_header->hdr_p_label_list[idx_label].hlbl_addr);
  }
  result = bhdr_raw_write(fout);
  return result;
}
