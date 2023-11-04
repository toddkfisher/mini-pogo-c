#pragma once

#define MAX_HEADER_SIZE 4096  // bytes
#define MAX_CODE_SIZE 4096  // instructions
#define MAX_TASK_DECLS 128

enum OPCODE
{
  OP_END_TASK = 0,
  OP_PUSH_CONST_INT,
  OP_POP_INT,
  OP_NEGATE,
  OP_OR,
  OP_AND,
  OP_NOT,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_REMAINDER,
  OP_GT,
  OP_LT,
  OP_GE,
  OP_LE,
  OP_EQ,
  OP_NE,
  OP_JUMP,
  OP_JUMP_IF_ZERO,
  OP_JUMP_IF_NONZERO,
  OP_BEGIN_SPAWN,
  OP_SPAWN,
  OP_END_SPAWN,
  OP_PRINT_INT,
  OP_PRINT_CHAR,
  OP_PUSH_VAR,
  OP_BAD = 0xff
};

typedef struct INSTRUCTION
{
  uint8_t i_opcode;
  union
  {
    // OP_PUSH_CONST_INT
    int32_t i_const_int;

    // OP_POP_TO_VAR, OP_PUSH_VAR
    uint8_t i_var_name;

    // OP_JUMP
    // OP_JUMP_IF_ZERO
    // OP_JUMP_IF_NONZERO
    uint32_t i_jump_addr;

    // OP_SPAWN
    uint32_t i_task_addr;

    // OP_PRINT_CHAR
    uint8_t i_char;

    // OP_ADD, OP_SUBTRACT, OP_MULTIPLY, OP_DIVIDE, OP_REMAINDER,
    // OP_GT, OP_LT, OP_GE, OP_LE, OP_EQ, OP_NE, OP_BEGIN_SPAWN,
    // OP_END_SPAWN, OP_PRINT_INT, OP_END_TASK
    //
    // no operands
  };
} INSTRUCTION;

void compile(PARSE_NODE *p_tree);
