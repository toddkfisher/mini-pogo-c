#pragma once

#define HEADER_SIZE_IDX 0
#define HEADER_N_LABELS_IDX (sizeof(uint32_t))
#define HEADER_CODE_SIZE_IDX (HEADER_N_LABELS_IDX + sizeof(uint32_t))
#define HEADER_MODULE_NAME_IDX (HEADER_CODE_SIZE_IDX + sizeof(uint32_t))
#define MAX_HEADER_SIZE 4096  // bytes

void bhdr_add_u32_to_header(uint32_t u32);
void bhdr_add_u8_to_header(uint8_t u8);
void bhdr_add_counted_string_to_header(char *str);
void bhdr_poke_u32_to_header(uint32_t u32, uint32_t ofs);
uint32_t bhdr_read_header_size(FILE *fin);
uint32_t bhdr_get_u32(uint32_t ofs);
uint32_t bhdr_get_label_count(void);
uint32_t bhdr_get_counted_string(uint32_t ofs, char *cstr);
uint32_t bhdr_write(FILE *fout);
uint32_t bhdr_read(FILE *fin, uint32_t n_header_bytes);
uint32_t bhdr_get_counted_string_size(uint32_t ofs);
void bhdr_init(void);
uint8_t bhdr_get_u8(uint32_t ofs);
uint32_t bhdr_get_label_list_ofs(void);
uint32_t bhdr_get_bytes_added(void);
uint32_t bhdr_get_code_size_in_bytes(void);
