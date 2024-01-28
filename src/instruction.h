#pragma once

#define MAX_CODE_SIZE 4096  // instructions

#include <enum-int.h>
enum OPCODE
{
  #include "opcode-enums.txt"
};

typedef struct INSTRUCTION {
  uint8_t i_opcode;
  union
  {
    // OP_PUSH_CONST_INT
    int32_t i_const_int;
    // OP_POP_INT, OP_PUSH_VAR
    uint8_t i_var_name;
    // OP_JUMP
    // OP_JUMP_IF_ZERO
    // OP_JUMP_IF_NONZERO
    uint32_t i_jump_addr;
    // OP_BEGIN_SPAWN
    uint32_t i_n_spawn_tasks;
    // OP_SPAWN
    uint32_t i_task_addr;
    // OP_PRINT_CHAR
    uint8_t i_char;
    // OP_ADD, OP_SUBTRACT, OP_MULTIPLY,  OP_DIVIDE, OP_REMAINDER, OP_GT, OP_LT,
    // OP_GE, OP_LE, OP_EQ, OP_NE, OP_END_SPAWN, OP_PRINT_INT, OP_END_TASK, OP_SLEEP
    //
    // no operands
  };
} INSTRUCTION;
