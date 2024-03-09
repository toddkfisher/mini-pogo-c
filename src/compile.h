#pragma once
//------------------------------------------------------------------------------ //
void compile_init(void);
void compile(PARSE_NODE *p_tree);
uint32_t compile_write_header(FILE *fout);
uint32_t compile_write_code(FILE *fout);
