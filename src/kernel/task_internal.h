#ifndef AYRENOS_TASK_INTERNAL_H
#define AYRENOS_TASK_INTERNAL_H

#include <stdbool.h>
#include "task.h"

#define TASK_STACK_FILL 0xA5A5A5A5U
#define TASK_STACK_CANARY 0xC0DEF00DU

typedef enum {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_BLOCKED
} task_state_t;

typedef enum {
    TASK_WAIT_NONE = 0,
    TASK_WAIT_SEND,
    TASK_WAIT_RECEIVE,
    TASK_WAIT_SEMAPHORE,
    TASK_WAIT_MUTEX
} task_wait_t;

typedef struct {
    uint32_t *sp;
    uint32_t *stack_base;
    size_t stack_words;

    uint32_t wake_tick;
    uint8_t priority;
    uint8_t base_priority;
    task_state_t state;

    task_wait_t wait_kind;
    void *wait_object;
    void *wait_buffer;
    bool wait_forever;
    task_result_t wait_result;
    uint32_t wait_order;
} task_control_t;

extern task_control_t *scheduler_current;

task_control_t *task_internal_get(task_id_t id);
size_t task_internal_count(void);
void task_internal_seal_creation(void);
void task_internal_prepare(task_control_t *task, const task_config_t *config);

void scheduler_select(void);
void scheduler_tick(void);
void scheduler_block(void *object, task_wait_t kind, void *buffer, uint32_t ticks);
void scheduler_wake(task_control_t *task, task_result_t result);
void sync_internal_refresh_priorities(void);

bool task_port_in_handler(void);
bool task_port_can_block(void);
uint32_t *task_port_prepare_stack(const task_config_t *config);
bool task_port_init(uint32_t clock_hz, uint32_t tick_hz);
void task_port_start_tick(void);
void task_port_request_switch(void);
void task_port_idle(void);
_Noreturn void task_port_start_first(void);

#endif /* AYRENOS_TASK_INTERNAL_H */
