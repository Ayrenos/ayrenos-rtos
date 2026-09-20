#ifndef AYRENOS_TASK_H
#define AYRENOS_TASK_H

#include <stdint.h>
#include <stddef.h>

#define TASK_MAX_COUNT 8U
#define TASK_MIN_STACK_WORDS 64U
#define TASK_PRIORITY_MIN 1U
#define TASK_PRIORITY_MAX 7U

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
    TASK_ERR_CONTEXT
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

#endif /* AYRENOS_TASK_H */