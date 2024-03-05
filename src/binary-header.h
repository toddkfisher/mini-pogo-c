#pragma once


typedef struct HEADER_LABEL HEADER_LABEL;
struct HEADER_LABEL
{
  char hlbl_name[MAX_STR];
  uint8_t hlbl_type;  // (0 == jump label, !0 == task label)
  uint32_t hlbl_addr;  // Address of  this lable relative to  0th instruciton in
                       // code.
};


typedef struct HEADER HEADER;
struct HEADER
{
  uint32_t hdr_size_bytes;
  uint32_t hdr_n_labels;
  uint32_t hdr_n_strings;
  uint32_t hdr_code_size_bytes; // How many bytes of P-machine code follows header.
  char hdr_module_name[MAX_STR];
  char **hdr_p_string_list;  // List of string constants that occur in mini-pogo
                             // source module.
  HEADER_LABEL *hdr_p_label_list;  // List of labels described above.
};


void bhdr_print_struct(HEADER *p_header);
uint32_t bhdr_write(FILE *fout, HEADER *p_header);
HEADER *bhdr_read(FILE *fin);
