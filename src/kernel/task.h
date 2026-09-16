#ifndef AYRENOS_TASK_H
#define AYRENOS_TASK_H

#include <stdint.h>

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING
} task_state_t;

typedef struct {
    uint32_t *sp; // stack pointer
    task_state_t task_state;
    uint32_t sys_tick;
} task_t;

#endif /* AYRENOS_TASK_H */