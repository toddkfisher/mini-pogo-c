#pragma once

#define STACK_SIZE 256
#define MAX_CHANNELS_PER_TASK 10

// Task states.
enum
{
  ST_RUNNING,
  ST_STOPPED,
  ST_BLOCKED
};

// Block flags
enum
{
  B_JOIN = 1  // Currently the only way to block a task.
};

typedef struct TASK TASK;
typedef struct MODULE MODULE;

struct TASK
{
  char task_name[MAX_STR];
  MODULE *task_p_module;               // Module that this task belongs to.
  int32_t task_variables[26];          // A-Z cheesy variables per task.
  int32_t task_stack[STACK_SIZE];      // Mini-pogo runs on a stack machine.
  uint32_t task_stack_top;
  uint32_t task_ip;                    // Instruction pointer to module code block.
  uint32_t task_state;
  uint32_t task_state_flags;
  uint32_t task_join_list_size;
  TASK **task_p_join_list;             // List of tasks that we're blocked on (join).
};

void exec_run_task(TASK *p_task);
void exec_run_module_at_init_code(char *module_file_name);
