#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <unistd.h>
#include <pthread.h>
#include <util.h>


#include "instruction.h"
#include "binary-header.h"
#include "instruction.h"
#include "exec.h"
#include "module.h"


#define PUSH(p_task, x) (p_task)->task_stack[(p_task)->task_stack_top++] = (x)
#define POP(p_task) (p_task)->task_stack[--(p_task)->task_stack_top]
#define STACK_PEEK(p_task, offset) (p_task)->task_stack[(p_task)->task_stack_top - 1 - (offset)]
#define STACK_DROP(p_task) --((p_task)->task_stack_top)


#define U64_LO_U32(x) ((x) & (uint64_t) 0xffffffff)
#define U64_HI_U32(x) (((x) >> 32) & (uint64_t) 0xffffffff)
#define U32x2_TO_U64(lo, hi) ((((uint64_t) (hi)) << 32) | ((uint64_t) lo))


void *exec_run_task(void *pv_task);


uint32_t g_n_tasks_created = 0;


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
  result->task_n_spawn_tasks = 0;
  g_n_tasks_created += 1;
  return result;
}


void exec_add_spawn_task(TASK *p_parent_task, uint32_t child_task_addr)
{
  TASK *p_child_task = NULL;
  char child_task_name[MAX_STR];
  HEADER *p_header = p_parent_task->task_p_module->mod_p_header;
  // TODO: Need a more efficient way of finding the task name.
  for (uint32_t i = 0; i < p_header->hdr_n_labels; ++i)
  {
    if (child_task_addr == p_header->hdr_p_label_list[i].hlbl_addr)
    {
      sprintf(child_task_name, "%s:%u", p_header->hdr_p_label_list[i].hlbl_name, g_n_tasks_created);
      break;
    }
  }
  p_child_task = exec_create_task(child_task_name,
                                  p_parent_task->task_p_module,
                                  child_task_addr);
  PUSH(p_parent_task, U64_LO_U32((uint64_t) p_child_task));
  PUSH(p_parent_task, U64_HI_U32((uint64_t) p_child_task));
}


void *exec_top_of_stack_to_ptr(TASK *p_task, uint32_t stk_offset)
{
  uint32_t u32_hi = STACK_PEEK(p_task, 0 + stk_offset);
  uint32_t u32_lo = STACK_PEEK(p_task, 1 + stk_offset);
  void *result = (void *) U32x2_TO_U64(u32_lo, u32_hi);
  return result;
}


void exec_run_then_join_spawn(TASK *p_parent_task)
{
  uint32_t stk_offset = 0;
  for (uint32_t i = 0; i < p_parent_task->task_n_spawn_tasks; ++i)
  {
    TASK *p_child_task = (TASK *) exec_top_of_stack_to_ptr(p_parent_task, stk_offset);
    pthread_create(&(p_child_task->task_thread_id),
                   NULL,
                   exec_run_task,
                   p_child_task);
    stk_offset += sizeof(void *)/sizeof(uint32_t);  // Look at next pointer in stack.
  }
  for (uint32_t i = 0; i < p_parent_task->task_n_spawn_tasks; ++i)
  {
    TASK *p_child_task = (TASK *) exec_top_of_stack_to_ptr(p_parent_task, 0);
    pthread_join(p_child_task->task_thread_id, NULL);
    free(p_child_task);
    STACK_DROP(p_parent_task);
    STACK_DROP(p_parent_task);
  }
}


#define BINARY_OP(operator)                                       \
  do                                                              \
  {                                                               \
    x = POP(p_task);                                              \
    y = POP(p_task);                                              \
    PUSH(p_task, y operator x);                                   \
  } while (0)


void *exec_run_task(void *pv_task)
{
  int32_t x;
  int32_t y;
  TASK *p_task = (TASK *) pv_task;
  MODULE *p_module = p_task->task_p_module;
  INSTRUCTION *p_code = p_module->mod_p_code;
  p_task->task_state = ST_RUNNING;
  while (ST_STOPPED != p_task->task_state)
  {
    INSTRUCTION *p_instruction = p_code + p_task->task_ip;
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
        //p_task->task_stack[p_task->task_stack_top - 1] = !p_task->task_stack[p_task->task_stack_top - 1];
        STACK_PEEK(p_task, 0) = !STACK_PEEK(p_task, 0);
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
        p_task->task_n_spawn_tasks = p_code[p_task->task_ip].i_n_spawn_tasks;
        p_task->task_ip += 1;
        break;
      case OP_SPAWN:
        exec_add_spawn_task(p_task, p_code[p_task->task_ip].i_task_addr);
        p_task->task_ip += 1;
        break;
      case OP_END_SPAWN:
        exec_run_then_join_spawn(p_task);
        p_task->task_ip += 1;
        break;
      case OP_PRINT_INT:
        printf("%d\n", POP(p_task));
        p_task->task_ip += 1;
        break;
      case OP_PRINT_CHAR:
        printf("%c", p_instruction->i_char);
        p_task->task_ip += 1;
        break;
      case OP_PUSH_VAR:
        PUSH(p_task, p_task->task_variables[p_instruction->i_var_name]);
        p_task->task_ip += 1;
        break;
      case OP_SLEEP:
        x = POP(p_task);
        if (x < 0)
          x = -x;
        p_task->task_state = ST_SLEEPING;
        sleep((unsigned int) x);
        p_task->task_state = ST_RUNNING;
        p_task->task_ip += 1;
        break;
      case OP_TEST_AND_JUMP_IF_ZERO:
        if (0 == STACK_PEEK(p_task, 0))
          p_task->task_ip = p_code[p_task->task_ip].i_jump_addr; //p_task->task_p_module->mod_p_code[p_task->task_ip].i_jump_addr;
        else
        {
          STACK_DROP(p_task);
          p_task->task_ip += 1;
        }
        break;
      case OP_TEST_AND_JUMP_IF_NONZERO:
        if (0 != STACK_PEEK(p_task, 0))
          p_task->task_ip = p_code[p_task->task_ip].i_jump_addr; //  p_task->task_p_module->mod_p_code[p_task->task_ip].i_jump_addr;
        else
        {
          STACK_DROP(p_task);
          p_task->task_ip += 1;
        }
        break;
      case OP_BAD:
        printf("OP_BAD\n");
        exit(0);
        break;
      default:
        break;
    }
  }
  pthread_exit(NULL);
}


// Load module and run it's 'init' code block.
void exec_run_module_at_init_code(char *module_file_name)
{
  FILE *fin = fopen(module_file_name, "r");
  MODULE *p_module;
  if (fin)
  {
    p_module = module_read(fin);
    if (p_module)
    {
      char init_task_name[MAX_STR];
      sprintf(init_task_name, "%s.<init>:%u", p_module->mod_p_header->hdr_module_name, g_n_tasks_created);
      p_module->mod_p_init_task = exec_create_task(init_task_name, p_module, 0);
      pthread_create(&(p_module->mod_p_init_task->task_thread_id),
                     NULL,
                     exec_run_task,
                     p_module->mod_p_init_task);
      pthread_join(p_module->mod_p_init_task->task_thread_id, NULL);
      module_free(p_module);
    }
    fclose(fin);
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
