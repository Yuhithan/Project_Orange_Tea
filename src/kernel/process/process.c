#include "process.h"

static process_t table[PROCESS_MAX];
static uint32_t next_pid;
static int last_slot;
void process_init(void) { for (int i = 0; i < PROCESS_MAX; i++) table[i].state = PROCESS_UNUSED; next_pid = 1; last_slot = 0; table[0].pid = next_pid++; table[0].parent_pid = 0; table[0].state = PROCESS_RUNNING; }
process_t *process_get(uint32_t pid) { for (int i = 0; i < PROCESS_MAX; i++) if (table[i].state != PROCESS_UNUSED && table[i].pid == pid) return &table[i]; return 0; }
process_t *process_create(uint32_t parent_pid) { if (parent_pid && !process_get(parent_pid)) return 0; for (int i = 0; i < PROCESS_MAX; i++) if (table[i].state == PROCESS_UNUSED) { table[i].pid = next_pid++; table[i].parent_pid = parent_pid; table[i].state = PROCESS_READY; table[i].exit_status = 0; return &table[i]; } return 0; }
int process_exit(uint32_t pid, int status) { process_t *p = process_get(pid); if (!p || p->pid == 1) return -1; p->exit_status = status; p->state = PROCESS_ZOMBIE; return 0; }
int process_wait(uint32_t parent_pid, uint32_t pid, int *status) { process_t *p = process_get(pid); if (!p || p->parent_pid != parent_pid || p->state != PROCESS_ZOMBIE) return -1; if (status) *status = p->exit_status; p->state = PROCESS_UNUSED; return 0; }
process_t *process_schedule_next(void) { for (int n = 0; n < PROCESS_MAX; n++) { int i = (last_slot + n + 1) % PROCESS_MAX; if (table[i].state == PROCESS_READY || table[i].state == PROCESS_RUNNING) { if (table[last_slot].state == PROCESS_RUNNING) table[last_slot].state = PROCESS_READY; table[i].state = PROCESS_RUNNING; last_slot = i; return &table[i]; } } return 0; }
