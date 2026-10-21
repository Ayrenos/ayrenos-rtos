#ifndef AYRENOS_SYNC_H
#define AYRENOS_SYNC_H

#include "task.h"

typedef struct {
    uint8_t *storage;
    size_t capacity;
    size_t item_size;
    size_t head;
    size_t count;
} queue_t;

typedef struct {
    uint32_t count;
    uint32_t maximum;
} semaphore_t;

typedef struct {
    void *owner;
    uint32_t initialized;
} mutex_t;

task_result_t queue_init(queue_t *queue, void *storage, size_t capacity, size_t item_size);
task_result_t semaphore_init(semaphore_t *semaphore, uint32_t initial, uint32_t maximum);
task_result_t mutex_init(mutex_t *mutex);

task_result_t queue_send(queue_t *queue, const void *item, uint32_t timeout);
task_result_t queue_receive(queue_t *queue, void *item, uint32_t timeout);
task_result_t semaphore_take(semaphore_t *semaphore, uint32_t timeout);
task_result_t semaphore_give(semaphore_t *semaphore);
task_result_t mutex_lock(mutex_t *mutex, uint32_t timeout);
task_result_t mutex_unlock(mutex_t *mutex);

task_result_t queue_send_isr(queue_t *queue, const void *item);
task_result_t queue_receive_isr(queue_t *queue, void *item);
task_result_t semaphore_give_isr(semaphore_t *semaphore);

#endif
