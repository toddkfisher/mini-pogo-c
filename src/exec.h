#pragma once
//------------------------------------------------------------------------------
#define STACK_SIZE 256
#define MAX_CHANNELS_PER_TASK 10
//------------------------------------------------------------------------------
// Task states.
enum
{
  ST_STOPPED = 0x00,
  ST_RUNNING = 0x01,
  ST_SLEEPING = 0x02,
  ST_BLOCKED = 0x03
};
//------------------------------------------------------------------------------
// Block flags
enum
{
  B_JOIN = 1  // Currently the only way to block a task.
};
//------------------------------------------------------------------------------
typedef struct TASK TASK;
typedef struct MODULE MODULE;
//------------------------------------------------------------------------------
struct TASK
{
  pthread_t task_thread_id;
  char task_name[MAX_STR];
  MODULE *task_p_module;               // Module that this task belongs to.
  int32_t task_variables[26];          // A-Z cheesy variables per task.
  int32_t task_stack[STACK_SIZE];      // Mini-pogo runs on a stack machine.
  uint32_t task_stack_top;
  uint32_t task_ip;                    // Instruction pointer to module code block.
  uint32_t task_state;
  uint32_t task_state_flags;
  uint32_t task_n_spawn_tasks;
};
