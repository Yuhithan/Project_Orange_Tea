#pragma once

#include <stdint.h>

#define PROCESS_MAX 16
typedef enum { PROCESS_UNUSED, PROCESS_READY, PROCESS_RUNNING, PROCESS_ZOMBIE } process_state_t;
typedef struct { uint32_t pid, parent_pid; process_state_t state; int exit_status; } process_t;
void process_init(void);
process_t *process_create(uint32_t parent_pid);
process_t *process_get(uint32_t pid);
int process_exit(uint32_t pid, int status);
int process_wait(uint32_t parent_pid, uint32_t pid, int *status);
process_t *process_schedule_next(void);
