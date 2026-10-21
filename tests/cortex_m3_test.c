#include "kernel/task.h"
#include "kernel/task_internal.h"
#include "kernel/scheduler.h"
#include "kernel/sync.h"
#include "kernel/critical.h"
#include "kernel/debug.h"
#include <stdbool.h>

static void output(const char *message)
{
    register uint32_t operation __asm("r0") = 4;
    register const char *argument __asm("r1") = message;
    __asm volatile ("bkpt #0xAB" : "+r" (operation) : "r" (argument) : "memory");
}

static _Noreturn void finish(uint32_t status)
{
    uint32_t arguments[2] = { 0x20026U, status };
    register uint32_t operation __asm("r0") = 0x20;
    register const void *argument __asm("r1") = arguments;
    __asm volatile ("bkpt #0xAB" : "+r" (operation) : "r" (argument) : "memory");
    for (;;) {}
}

#define CHECK(expression) do { \
    if (!(expression)) { output("FAIL: " #expression "\n"); finish(1); } \
} while (0)

extern bool test_registers(void);

_Alignas(8) static uint32_t stacks[3][384];
static semaphore_t gates[2];
static semaphore_t release_owner;
static semaphore_t empty;
static queue_t queue;
static uint32_t queue_storage[1];
static mutex_t mutex;
static volatile uint32_t command[2];
static volatile uint32_t progress[2];
static volatile uint32_t done[2];
static volatile bool run_busy;
static volatile bool owner_ready;
static volatile bool inherited;
static volatile task_result_t results[2];
static volatile uint32_t received;
static task_id_t ids[3];

static void worker(void *argument)
{
    size_t id = (size_t)argument;

    for (;;) {
        CHECK(semaphore_take(&gates[id], TASK_WAIT_FOREVER) == TASK_OK);
        uint32_t value = 42;

        switch (command[id]) {
        case 1:
            while (run_busy) {
                ++progress[id];
            }
            break;
        case 2:
            for (size_t i = 0; i < 100; ++i) {
                CHECK(test_registers());
                ++progress[id];
            }
            break;
        case 3:
            results[id] = queue_receive(&queue, &value, TASK_WAIT_FOREVER);
            received = value;
            break;
        case 4:
            results[id] = queue_send(&queue, &value, TASK_WAIT_FOREVER);
            break;
        case 5:
            results[id] = semaphore_take(&empty, TASK_WAIT_FOREVER);
            break;
        case 6:
            CHECK(mutex_lock(&mutex, TASK_WAIT_FOREVER) == TASK_OK);
            owner_ready = true;
            CHECK(semaphore_take(&release_owner, TASK_WAIT_FOREVER) == TASK_OK);
            inherited = scheduler_current->priority == 7;
            CHECK(mutex_unlock(&mutex) == TASK_OK);
            break;
        case 7:
            CHECK(task_sleep(5) == TASK_OK);
            CHECK(semaphore_give(&release_owner) == TASK_OK);
            break;
        default:
            CHECK(false);
        }

        ++done[id];
    }
}

static void dispatch(size_t id, uint32_t operation)
{
    command[id] = operation;
    CHECK(semaphore_give(&gates[id]) == TASK_OK);
}

static void await_done(size_t id, uint32_t expected)
{
    uint32_t begin = kernel_ticks();

    while (done[id] < expected) {
        CHECK((uint32_t)(kernel_ticks() - begin) < 100);
        CHECK(task_sleep(1) == TASK_OK);
    }
}

static void controller(void *argument)
{
    (void)argument;
    CHECK(scheduler_current == task_internal_get(ids[0]));
    CHECK(task_yield() == TASK_OK);
    CHECK(scheduler_current == task_internal_get(ids[0]));
    run_busy = true;
    dispatch(0, 1);
    dispatch(1, 1);
    CHECK(task_sleep(10) == TASK_OK);
    CHECK(progress[0] > 0 && progress[1] > 0);
    run_busy = false;
    await_done(0, 1);
    await_done(1, 1);

    dispatch(0, 2);
    dispatch(1, 2);
    await_done(0, 2);
    await_done(1, 2);
    CHECK(progress[0] >= 100 && progress[1] >= 100);

    uint32_t value = 99;
    dispatch(0, 3);
    CHECK(task_sleep(2) == TASK_OK);
    CHECK(task_internal_get(ids[1])->state == TASK_BLOCKED);
    CHECK(queue_send(&queue, &value, 0) == TASK_OK);
    await_done(0, 3);
    CHECK(results[0] == TASK_OK && received == 99);
    CHECK(queue_send(&queue, &value, 0) == TASK_OK);
    dispatch(0, 4);
    CHECK(task_sleep(2) == TASK_OK);
    CHECK(task_internal_get(ids[1])->state == TASK_BLOCKED);
    CHECK(queue_receive(&queue, &value, 0) == TASK_OK && value == 99);
    await_done(0, 4);
    CHECK(queue_receive(&queue, &value, 0) == TASK_OK && value == 42);
    CHECK(queue_receive(&queue, &value, 2) == TASK_ERR_TIMEOUT);
    CHECK(semaphore_take(&empty, 2) == TASK_ERR_TIMEOUT);

    dispatch(0, 5);
    CHECK(task_sleep(2) == TASK_OK);
    CHECK(semaphore_give(&empty) == TASK_OK);
    await_done(0, 5);
    CHECK(results[0] == TASK_OK);

    dispatch(0, 6);
    CHECK(task_sleep(2) == TASK_OK);
    CHECK(owner_ready);
    dispatch(1, 7);
    CHECK(mutex_lock(&mutex, 50) == TASK_OK);
    CHECK(inherited);
    CHECK(mutex_unlock(&mutex) == TASK_OK);
    await_done(0, 6);
    await_done(1, 3);
    CHECK(task_internal_get(ids[1])->priority == 2);
    CHECK(mutex_lock(&mutex, 0) == TASK_OK);
    CHECK(mutex_lock(&mutex, 0) == TASK_ERR_DEADLOCK);
    CHECK(mutex_unlock(&mutex) == TASK_OK);

    uint32_t mask = kernel_critical_enter();
    uint32_t inner = kernel_critical_enter();
    kernel_critical_exit(inner);
    CHECK(task_sleep(1) == TASK_ERR_CONTEXT);
    kernel_critical_exit(mask);
    for (size_t i = 0; i < 3; ++i) {
        size_t words;
        CHECK(task_stack_usage(ids[i], &words) == TASK_OK);
        CHECK(words > 16 && words < 384);
    }
    CHECK(kernel_panic_reason == KERNEL_PANIC_NONE);
    output("PASS: Cortex-M3 startup, preemption, yield/registers, sleep/idle, blocking queues, timeouts, semaphores, priority inheritance, stack usage\n");
    finish(0);
}

int main(void)
{
    CHECK(queue_init(&queue, queue_storage, 1, sizeof(uint32_t)) == TASK_OK);
    CHECK(semaphore_init(&empty, 0, 1) == TASK_OK);
    CHECK(semaphore_init(&release_owner, 0, 1) == TASK_OK);
    CHECK(mutex_init(&mutex) == TASK_OK);
    for (size_t i = 0; i < 2; ++i) {
        CHECK(semaphore_init(&gates[i], 0, 1) == TASK_OK);
    }
    for (size_t i = 0; i < 3; ++i) {
        const task_config_t config = {
            .entry = i == 0 ? controller : worker,
            .argument = (void *)(i == 0 ? 0 : i - 1),
            .stack = stacks[i],
            .stack_words = 384,
            .priority = i == 0 ? 7 : 2
        };
        CHECK(task_create(&config, &ids[i]) == TASK_OK);
    }
    kernel_start();
}
