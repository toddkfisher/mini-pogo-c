#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <unistd.h>
#include <threads.h>
#include <util.h>

#include "instruction.h"
#include "binary-header.h"
#include "instruction.h"
#include "module.h"
#include "exec.h"

uint32_t g_n_tasks = 0;

TASK *exec_create_task(char *name, MODULE *p_module, uint32_t ip)
{
  TASK *result = malloc(sizeof(TASK));
  result->task_p_module = p_module;
  strcpy(result->task_name, name);
  for (char v = 'A'; v <= 'Z'; ++v)
    result->task_variables[(uint32_t) v] = 0;
  result->task_stack_top = 0;
  result->task_ip = ip;
  result->task_state = ST_STOPPED;
  return result;
}

// Load module and run it's 'init' code block.
void exec_run_module_at_init_code(char *module_file_name)
{
  FILE *fin = fopen(module_file_name, "r");
  MODULE *p_module;
  if (NULL != fin)
  {
    p_module = module_read(fin);
    if (NULL != p_module)
    {
      TASK *p_task = exec_create_task("", p_module, 0);
      exec_run_task(p_task);
      module_free(p_module);
    }
    fclose(fin);
  }
}

#define PUSH(p_task, x) (p_task)->task_stack[p_task->task_stack_top++] = (x)
#define POP(p_task) p_task->task_stack[--p_task->task_stack_top]

#define BINARY_OP(operator)                                       \
  do                                                              \
  {                                                               \
    x = POP(p_task);                                              \
    y = POP(p_task);                                              \
    PUSH(p_task, y operator x);                                   \
  } while (0)

void exec_run_task(TASK *p_task)
{
  int32_t x;
  int32_t y;
  p_task->task_state = ST_RUNNING;
  while (ST_STOPPED != p_task->task_state)
  {
    INSTRUCTION *p_instruction = p_task->task_p_module->mod_p_code + p_task->task_ip;
    switch (p_instruction->i_opcode)
    {
      case OP_PUSH_CONST_INT:
        PUSH(p_task, p_instruction->i_const_int);
        p_task->task_ip += 1;
        break;
      case OP_END_TASK:
        p_task->task_state = ST_STOPPED;
        break;
      case OP_POP_INT:
        p_task->task_variables[p_instruction->i_var_name] = POP(p_task);
        p_task->task_ip += 1;
        break;
      case OP_NEGATE:
        p_task->task_stack[p_task->task_stack_top - 1] *= -1;
        p_task->task_ip += 1;
        break;
      case OP_OR:
        BINARY_OP(||);
        p_task->task_ip += 1;
        break;
      case OP_AND:
        BINARY_OP(&&);
        p_task->task_ip += 1;
        break;
      case OP_NOT:
        p_task->task_stack[p_task->task_stack_top - 1] = !p_task->task_stack[p_task->task_stack_top - 1];
        p_task->task_ip += 1;
        break;
      case OP_ADD:
        BINARY_OP(+);
        p_task->task_ip += 1;
        break;
      case OP_SUBTRACT:
        BINARY_OP(-);
        p_task->task_ip += 1;
        break;
      case OP_MULTIPLY:
        BINARY_OP(*);
        p_task->task_ip += 1;
        break;
      case OP_DIVIDE:
        BINARY_OP(/);
        p_task->task_ip += 1;
        break;
      case OP_REMAINDER:
        BINARY_OP(%);
        p_task->task_ip += 1;
        break;
      case OP_GT:
        BINARY_OP(>);
        p_task->task_ip += 1;
        break;
      case OP_LT:
        BINARY_OP(<);
        p_task->task_ip += 1;
        break;
      case OP_GE:
        BINARY_OP(>=);
        p_task->task_ip += 1;
        break;
      case OP_LE:
        BINARY_OP(<=);
        p_task->task_ip += 1;
        break;
      case OP_EQ:
        BINARY_OP(==);
        p_task->task_ip += 1;
        break;
      case OP_NE:
        BINARY_OP(!=);
        p_task->task_ip += 1;
        break;
      case OP_JUMP:
        p_task->task_ip = p_instruction->i_jump_addr;
        break;
      case OP_JUMP_IF_ZERO:
        if (POP(p_task))
          p_task->task_ip += 1;  // No jump
        else
          p_task->task_ip = p_instruction->i_jump_addr;
        break;
      case OP_JUMP_IF_NONZERO:
        if (POP(p_task))
          p_task->task_ip = p_instruction->i_jump_addr;
        else
          p_task->task_ip += 1;  // No jump
        break;
      case OP_BEGIN_SPAWN:
        // create spawn array (malloc).
        // spawn array idx = 0.
        p_task->task_ip += 1;
        break;
      case OP_SPAWN:
        // pthread_create(...).
        // spawn array[spawn idx++] = thread id
        p_task->task_ip += 1;
        break;
      case OP_END_SPAWN:
        // for (...) pthread_join(...)
        // free spawn array
        p_task->task_ip += 1;
        break;
      case OP_PRINT_INT:
        printf("%d\n", POP(p_task));
        p_task->task_ip += 1;
        break;
      case OP_PRINT_CHAR:
        if (isprint(p_instruction->i_char))
          printf("%c", p_instruction->i_char);
        else
          printf("%02x", p_instruction->i_char);
        p_task->task_ip += 1;
        break;
      case OP_PUSH_VAR:
        PUSH(p_task, p_task->task_variables[p_instruction->i_var_name]);
        p_task->task_ip += 1;
        break;
      case OP_BAD:
        printf("OP_BAD\n");
        exit(0);
        break;
      default:
        break;
    }
  }
}

int main(int argc, char **argv)
{
  if (argc != 2)
    fprintf(stderr, "usage: mpr <compiled module file>\n");
  else
  {
    if (0 != access(argv[1], R_OK))
      fprintf(stderr, "%s : doesn't exist\n", argv[1]);
    else
      exec_run_module_at_init_code(argv[1]);
  }
}
