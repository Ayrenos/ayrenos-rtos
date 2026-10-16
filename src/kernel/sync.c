#include "sync.h"
#include "task_internal.h"
#include "critical.h"
#include <stdbool.h>

#define MUTEX_INITIALIZED 0x4D555458U

static bool timeout_valid(uint32_t timeout)
{
    return timeout <= (uint32_t)TASK_WAIT_MAX || timeout == TASK_WAIT_FOREVER;
}

static task_result_t thread_context(uint32_t timeout)
{
    if (task_port_in_handler() || (timeout != 0U && !task_port_can_block())) {
        return TASK_ERR_CONTEXT;
    }

    if (!timeout_valid(timeout)) {
        return TASK_ERR_ARGUMENT;
    }

    return TASK_OK;
}

static void copy_item(void *destination, const void *source, size_t bytes)
{
    uint8_t *out = destination;
    const uint8_t *in = source;

    for (size_t i = 0; i < bytes; ++i) {
        out[i] = in[i];
    }
}

/* Highest effective priority first; equal priorities retain arrival order. */
static task_control_t *waiter(void *object, task_wait_t kind)
{
    task_control_t *best = NULL;

    for (size_t i = 0; i < task_internal_count(); ++i) {
        task_control_t *task = task_internal_get((task_id_t)i);

        if (task->state != TASK_BLOCKED || task->wait_object != object ||
            task->wait_kind != kind) {
            continue;
        }

        bool older = best != NULL && task->wait_order != best->wait_order &&
            (uint32_t)(best->wait_order - task->wait_order) <= (uint32_t)TASK_WAIT_MAX;

        if (best == NULL || task->priority > best->priority ||
            (task->priority == best->priority && older)) {
            best = task;
        }
    }

    return best;
}

/* Rebuild donations, including chains and tasks owning several mutexes. */
void sync_internal_refresh_priorities(void)
{
    size_t count = task_internal_count();

    for (size_t i = 0; i < count; ++i) {
        task_control_t *task = task_internal_get((task_id_t)i);
        task->priority = task->base_priority;
    }

    for (size_t pass = 0; pass < count; ++pass) {
        bool changed = false;

        for (size_t i = 0; i < count; ++i) {
            task_control_t *task = task_internal_get((task_id_t)i);

            if (task->state == TASK_BLOCKED && task->wait_kind == TASK_WAIT_MUTEX) {
                mutex_t *mutex = task->wait_object;
                task_control_t *owner = mutex->owner;

                if (owner != NULL && owner->priority < task->priority) {
                    owner->priority = task->priority;
                    changed = true;
                }
            }
        }

        if (!changed) {
            break;
        }
    }
}

static task_result_t block(void *object, task_wait_t kind, void *buffer,
                           uint32_t timeout, uint32_t mask)
{
    if (timeout == 0U) {
        kernel_critical_exit(mask);
        return TASK_ERR_TIMEOUT;
    }

    if (scheduler_current == NULL) {
        kernel_critical_exit(mask);
        return TASK_ERR_STATE;
    }

    task_control_t *task = scheduler_current;
    scheduler_block(object, kind, buffer, timeout);
    kernel_critical_exit(mask);

    /* PendSV runs before this task resumes; the wakeup stores its result. */
    return task->wait_result;
}

task_result_t queue_init(queue_t *queue, void *storage, size_t capacity, size_t item_size)
{
    if (task_port_in_handler()) {
        return TASK_ERR_CONTEXT;
    }

    if (scheduler_current != NULL) {
        return TASK_ERR_STATE;
    }

    if (queue == NULL || storage == NULL || capacity == 0U || item_size == 0U ||
        capacity > SIZE_MAX / item_size) {
        return TASK_ERR_ARGUMENT;
    }

    queue->storage = storage;
    queue->capacity = capacity;
    queue->item_size = item_size;
    queue->head = 0;
    queue->count = 0;
    return TASK_OK;
}

static bool queue_valid(const queue_t *queue)
{
    return queue != NULL && queue->storage != NULL && queue->capacity != 0U &&
        queue->item_size != 0U && queue->capacity <= SIZE_MAX / queue->item_size;
}

static task_result_t send(queue_t *queue, const void *item, uint32_t timeout)
{
    if (!queue_valid(queue) || item == NULL) {
        return TASK_ERR_ARGUMENT;
    }

    uint32_t mask = kernel_critical_enter();
    task_control_t *receiver = waiter(queue, TASK_WAIT_RECEIVE);

    if (receiver != NULL) {
        copy_item(receiver->wait_buffer, item, queue->item_size);
        scheduler_wake(receiver, TASK_OK);
    } else if (queue->count < queue->capacity) {
        size_t tail = (queue->head + queue->count) % queue->capacity;
        copy_item(queue->storage + tail * queue->item_size, item, queue->item_size);
        ++queue->count;
    } else {
        return block(queue, TASK_WAIT_SEND, (void *)item, timeout, mask);
    }

    kernel_critical_exit(mask);
    return TASK_OK;
}

static task_result_t receive(queue_t *queue, void *item, uint32_t timeout)
{
    if (!queue_valid(queue) || item == NULL) {
        return TASK_ERR_ARGUMENT;
    }

    uint32_t mask = kernel_critical_enter();

    if (queue->count == 0U) {
        return block(queue, TASK_WAIT_RECEIVE, item, timeout, mask);
    }

    copy_item(item, queue->storage + queue->head * queue->item_size, queue->item_size);
    queue->head = (queue->head + 1U) % queue->capacity;
    --queue->count;

    /* Complete a blocked send atomically, preventing another sender stealing space. */
    task_control_t *sender = waiter(queue, TASK_WAIT_SEND);

    if (sender != NULL) {
        size_t tail = (queue->head + queue->count) % queue->capacity;
        copy_item(queue->storage + tail * queue->item_size, sender->wait_buffer,
                  queue->item_size);
        ++queue->count;
        scheduler_wake(sender, TASK_OK);
    }

    kernel_critical_exit(mask);
    return TASK_OK;
}

task_result_t queue_send(queue_t *queue, const void *item, uint32_t timeout)
{
    task_result_t result = thread_context(timeout);
    return result == TASK_OK ? send(queue, item, timeout) : result;
}

task_result_t queue_receive(queue_t *queue, void *item, uint32_t timeout)
{
    task_result_t result = thread_context(timeout);
    return result == TASK_OK ? receive(queue, item, timeout) : result;
}

task_result_t queue_send_isr(queue_t *queue, const void *item)
{
    return task_port_in_handler() ? send(queue, item, 0) : TASK_ERR_CONTEXT;
}

task_result_t queue_receive_isr(queue_t *queue, void *item)
{
    return task_port_in_handler() ? receive(queue, item, 0) : TASK_ERR_CONTEXT;
}

task_result_t semaphore_init(semaphore_t *semaphore, uint32_t initial, uint32_t maximum)
{
    if (task_port_in_handler()) {
        return TASK_ERR_CONTEXT;
    }

    if (scheduler_current != NULL) {
        return TASK_ERR_STATE;
    }

    if (semaphore == NULL || maximum == 0U || initial > maximum) {
        return TASK_ERR_ARGUMENT;
    }

    semaphore->count = initial;
    semaphore->maximum = maximum;
    return TASK_OK;
}

task_result_t semaphore_take(semaphore_t *semaphore, uint32_t timeout)
{
    task_result_t result = thread_context(timeout);

    if (result != TASK_OK) {
        return result;
    }

    if (semaphore == NULL || semaphore->maximum == 0U) {
        return TASK_ERR_ARGUMENT;
    }

    uint32_t mask = kernel_critical_enter();

    if (semaphore->count == 0U) {
        return block(semaphore, TASK_WAIT_SEMAPHORE, NULL, timeout, mask);
    }

    --semaphore->count;
    kernel_critical_exit(mask);
    return TASK_OK;
}

static task_result_t give(semaphore_t *semaphore)
{
    if (semaphore == NULL || semaphore->maximum == 0U) {
        return TASK_ERR_ARGUMENT;
    }

    uint32_t mask = kernel_critical_enter();
    task_control_t *task = waiter(semaphore, TASK_WAIT_SEMAPHORE);

    if (task != NULL) {
        scheduler_wake(task, TASK_OK);
    } else if (semaphore->count < semaphore->maximum) {
        ++semaphore->count;
    } else {
        kernel_critical_exit(mask);
        return TASK_ERR_LIMIT;
    }

    kernel_critical_exit(mask);
    return TASK_OK;
}

task_result_t semaphore_give(semaphore_t *semaphore)
{
    return task_port_in_handler() ? TASK_ERR_CONTEXT : give(semaphore);
}

task_result_t semaphore_give_isr(semaphore_t *semaphore)
{
    return task_port_in_handler() ? give(semaphore) : TASK_ERR_CONTEXT;
}

task_result_t mutex_init(mutex_t *mutex)
{
    if (task_port_in_handler()) {
        return TASK_ERR_CONTEXT;
    }

    if (scheduler_current != NULL) {
        return TASK_ERR_STATE;
    }

    if (mutex == NULL) {
        return TASK_ERR_ARGUMENT;
    }

    mutex->owner = NULL;
    mutex->initialized = MUTEX_INITIALIZED;
    return TASK_OK;
}

task_result_t mutex_lock(mutex_t *mutex, uint32_t timeout)
{
    if (!task_port_can_block()) {
        return TASK_ERR_CONTEXT;
    }

    if (mutex == NULL || mutex->initialized != MUTEX_INITIALIZED || !timeout_valid(timeout)) {
        return TASK_ERR_ARGUMENT;
    }

    uint32_t mask = kernel_critical_enter();

    if (scheduler_current == NULL) {
        kernel_critical_exit(mask);
        return TASK_ERR_STATE;
    }

    task_control_t *owner = mutex->owner;

    if (owner == NULL) {
        mutex->owner = scheduler_current;
        kernel_critical_exit(mask);
        return TASK_OK;
    }

    /* Reject recursive locking and cycles in mutex ownership chains. */
    for (size_t i = 0; owner != NULL && i <= task_internal_count(); ++i) {
        if (owner == scheduler_current) {
            kernel_critical_exit(mask);
            return TASK_ERR_DEADLOCK;
        }

        if (owner->state != TASK_BLOCKED || owner->wait_kind != TASK_WAIT_MUTEX) {
            break;
        }

        mutex_t *next = owner->wait_object;
        owner = next->owner;
    }

    return block(mutex, TASK_WAIT_MUTEX, NULL, timeout, mask);
}

task_result_t mutex_unlock(mutex_t *mutex)
{
    if (!task_port_can_block()) {
        return TASK_ERR_CONTEXT;
    }

    if (mutex == NULL || mutex->initialized != MUTEX_INITIALIZED) {
        return TASK_ERR_ARGUMENT;
    }

    uint32_t mask = kernel_critical_enter();

    if (scheduler_current == NULL || mutex->owner != scheduler_current) {
        kernel_critical_exit(mask);
        return TASK_ERR_OWNERSHIP;
    }

    task_control_t *next = waiter(mutex, TASK_WAIT_MUTEX);
    mutex->owner = next;

    if (next != NULL) {
        scheduler_wake(next, TASK_OK);
    }

    sync_internal_refresh_priorities();
    task_port_request_switch();
    kernel_critical_exit(mask);
    return TASK_OK;
}
