#pragma once


#define MAX_CODE_SIZE 4096  // instructions


#include <enum-int.h>
enum OPCODE
{
  #include "opcode-enums.txt"
};


typedef struct INSTRUCTION INSTRUCTION;
struct INSTRUCTION {
  uint8_t i_opcode;
  union
  {
    // opcode: OP_PUSH_CONST_INT
    /* operand: */ int32_t i_const_int;
    // opcodes: OP_POP_INT,
    //          OP_PUSH_VAR
    /* operand: */ uint8_t i_var_name;
    // opcodes: OP_JUMP
    //          OP_JUMP_IF_ZERO
    //          OP_JUMP_IF_NONZERO
    //          OP_TEST_AND_JUMP_IF_ZERO
    //          OP_TEST_AND_JUMP_IF_NONZERO
    /* operand: */ uint32_t i_jump_addr;
    // opcode: OP_BEGIN_SPAWN
    /* operand: */ uint32_t i_n_spawn_tasks;
    // opcode: OP_SPAWN
    /* operand: */ uint32_t i_task_addr;
    // OP_PRINT_CHAR
    /* operand: */ uint8_t i_char;
    // opcode: OP_PRINT_STRING
    /* operand: */ uint32_t i_string_idx;  // Index in header.
    // opcodes: OP_ADD,
    //          OP_SUBTRACT,
    //          OP_MULTIPLY,
    //          OP_DIVIDE,
    //          OP_REMAINDER,
    //          OP_GT,
    //          OP_LT,
    //          OP_GE,
    //          OP_LE,
    //          OP_EQ,
    //          OP_NE,
    //          OP_END_SPAWN,
    //          OP_PRINT_INT,
    //          OP_END_TASK,
    //          OP_SLEEP
    //          OP_BEGIN_ATOMIC_PRINT
    //          OP_END_ATOMIC_PRINT
    /* no operands */
  };
};
