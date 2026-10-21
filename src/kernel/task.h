#ifndef AYRENOS_TASK_H
#define AYRENOS_TASK_H

#include <stdint.h>
#include <stddef.h>

#define TASK_MAX_COUNT 8U
#define TASK_MIN_STACK_WORDS 64U
#define TASK_PRIORITY_MIN 1U
#define TASK_PRIORITY_MAX 7U

#define TASK_WAIT_MAX INT32_MAX
#define TASK_WAIT_FOREVER UINT32_MAX

typedef uint16_t task_id_t;
typedef void (*task_entry_t)(void *argument);

typedef enum {
    TASK_OK = 0,
    TASK_ERR_ARGUMENT,
    TASK_ERR_STACK_ALIGNMENT,
    TASK_ERR_STACK_SIZE,
    TASK_ERR_STACK_IN_USE,
    TASK_ERR_PRIORITY,
    TASK_ERR_CAPACITY,
    TASK_ERR_KERNEL_STARTED,
    TASK_ERR_CONTEXT,
    TASK_ERR_STATE,
    TASK_ERR_TIMEOUT,
    TASK_ERR_LIMIT,
    TASK_ERR_OWNERSHIP,
    TASK_ERR_DEADLOCK
} task_result_t;

typedef struct {
    task_entry_t entry;
    void *argument;

    uint32_t *stack;
    size_t stack_words;

    uint8_t priority;
} task_config_t;

task_result_t task_create(
    const task_config_t *config,
    task_id_t *out_id
);

task_result_t task_yield(void);
task_result_t task_sleep(uint32_t ticks);

task_result_t task_stack_usage(task_id_t id, size_t *out_words);

#endif /* AYRENOS_TASK_H */
