#pragma once

// from: lex.[hc]
#include "lex.h"
void lex_set_input_function(INPUT_FUNCTION input_function, void *data);
bool lex_is_keyword(uint8_t lex_type);
bool lex_is_symbol(uint8_t lex_type);
void lex_print(LEXICAL_UNIT *p_lex);
int32_t lex_scan(void);
// Included for testing.
char lex_get_char(bool peek_char);
