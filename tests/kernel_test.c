#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include "../src/kernel/task.c"
#include "../src/kernel/scheduler.c"
#include "../src/kernel/sync.c"

static uint32_t interrupt_mask;
static bool handler_context;
static unsigned switches;
static jmp_buf panic_target;
static kernel_panic_t observed_panic;
static void (*resume_hook)(void);

uint32_t kernel_critical_enter(void)
{
    uint32_t previous = interrupt_mask;
    interrupt_mask = 1;
    return previous;
}

void kernel_critical_exit(uint32_t previous)
{
    interrupt_mask = previous;

    if (previous == 0U && resume_hook != NULL) {
        void (*hook)(void) = resume_hook;
        resume_hook = NULL;
        hook();
    }
}

bool task_port_in_handler(void) { return handler_context; }
bool task_port_can_block(void) { return !handler_context && interrupt_mask == 0U; }
uint32_t *task_port_prepare_stack(const task_config_t *config)
{
    uint32_t *sp = config->stack + config->stack_words - 16;
    for (size_t i = 0; i < 16; ++i) { sp[i] = 0; }
    return sp;
}
bool task_port_init(uint32_t clock, uint32_t hz) { return clock >= hz; }
void task_port_start_tick(void) {}
void task_port_request_switch(void) { ++switches; }
void task_port_idle(void) {}
_Noreturn void task_port_start_first(void) { assert(false); for (;;) {} }
_Noreturn void kernel_panic(kernel_panic_t reason)
{
    observed_panic = reason;
    longjmp(panic_target, 1);
}

_Alignas(8) static uint32_t stacks[TASK_MAX_COUNT][128];
static void entry(void *argument) { (void)argument; }

static void reset(size_t count)
{
    memset(tasks, 0, sizeof(tasks));
    used = 0;
    creation_sealed = false;
    scheduler_current = NULL;
    tick_count = 0;
    wait_sequence = 0;
    interrupt_mask = 0;
    handler_context = false;
    switches = 0;
    resume_hook = NULL;

    const task_config_t idle_config = { entry, NULL, idle_stack, 128, 0 };
    task_internal_prepare(&idle_task, &idle_config);

    for (size_t i = 0; i < count; ++i) {
        const task_config_t config = { entry, NULL, stacks[i], 128, 2 };
        task_id_t id;
        assert(task_create(&config, &id) == TASK_OK);
        assert(id == i);
    }
}

static void select_next(void)
{
    uint32_t mask = kernel_critical_enter();
    scheduler_select();
    kernel_critical_exit(mask);
}

static void tick(void)
{
    uint32_t mask = kernel_critical_enter();
    scheduler_tick();
    kernel_critical_exit(mask);
}

static void wait_on(size_t id, void *object, task_wait_t kind, void *buffer, uint32_t delay)
{
    scheduler_current = &tasks[id];
    tasks[id].state = TASK_RUNNING;
    uint32_t mask = kernel_critical_enter();
    scheduler_block(object, kind, buffer, delay);
    kernel_critical_exit(mask);
}

static void test_creation(void)
{
    reset(1);
    task_id_t id;
    task_config_t config = { entry, NULL, stacks[0], 128, 2 };
    assert(task_create(&config, &id) == TASK_ERR_STACK_IN_USE);
    config.stack = stacks[1];
    config.priority = 0;
    assert(task_create(&config, &id) == TASK_ERR_PRIORITY);
    config.priority = 2;
    config.stack_words = 63;
    assert(task_create(&config, &id) == TASK_ERR_STACK_SIZE);
    config.stack_words = 128;
    handler_context = true;
    assert(task_create(&config, &id) == TASK_ERR_CONTEXT);
    handler_context = false;
    task_internal_seal_creation();
    assert(task_create(&config, &id) == TASK_ERR_KERNEL_STARTED);
    reset(TASK_MAX_COUNT);
    config.stack = idle_stack;
    assert(task_create(&config, &id) == TASK_ERR_CAPACITY);
}

static void test_scheduling(void)
{
    reset(3);
    select_next();
    assert(scheduler_current == &tasks[0]);
    select_next();
    assert(scheduler_current == &tasks[1]);
    select_next();
    assert(scheduler_current == &tasks[2]);
    select_next();
    assert(scheduler_current == &tasks[0]);
    tasks[2].base_priority = 7;
    select_next();
    assert(scheduler_current == &tasks[2]);
    tasks[2].state = TASK_SLEEPING;
    tasks[0].state = TASK_SLEEPING;
    tasks[1].state = TASK_SLEEPING;
    select_next();
    assert(scheduler_current == &idle_task);
}

static void test_time_and_context(void)
{
    reset(1);
    select_next();
    tick_count = UINT32_MAX - 1U;
    assert(task_sleep(3) == TASK_OK);
    tick(); tick();
    assert(tasks[0].state == TASK_SLEEPING);
    tick();
    assert(tasks[0].state == TASK_READY);
    assert(kernel_ticks() == 1U);
    assert(task_sleep(TASK_WAIT_FOREVER) == TASK_ERR_ARGUMENT);
    handler_context = true;
    assert(task_yield() == TASK_ERR_CONTEXT);
    handler_context = false;
    uint32_t outer = kernel_critical_enter();
    uint32_t inner = kernel_critical_enter();
    kernel_critical_exit(inner);
    assert(interrupt_mask == 1U);
    assert(task_sleep(1) == TASK_ERR_CONTEXT);
    kernel_critical_exit(outer);
    assert(interrupt_mask == 0U);
}

static void test_queue(void)
{
    reset(2);
    queue_t queue = { 0 };
    uint32_t storage[2];
    uint32_t out = 0;
    uint32_t a = 11, b = 22, c = 33;
    assert(queue_init(&queue, storage, 2, sizeof(a)) == TASK_OK);
    assert(queue_receive(&queue, &out, 0) == TASK_ERR_TIMEOUT);
    assert(queue_send(&queue, &a, 0) == TASK_OK);
    assert(queue_send(&queue, &b, 0) == TASK_OK);
    assert(queue_send(&queue, &c, 0) == TASK_ERR_TIMEOUT);
    wait_on(0, &queue, TASK_WAIT_SEND, &c, 10);
    assert(queue_receive(&queue, &out, 0) == TASK_OK && out == a);
    assert(tasks[0].state == TASK_READY && tasks[0].wait_result == TASK_OK);
    assert(queue_receive(&queue, &out, 0) == TASK_OK && out == b);
    assert(queue_receive(&queue, &out, 0) == TASK_OK && out == c);
    wait_on(1, &queue, TASK_WAIT_RECEIVE, &out, TASK_WAIT_FOREVER);
    handler_context = true;
    assert(queue_send(&queue, &a, 0) == TASK_ERR_CONTEXT);
    assert(queue_send_isr(&queue, &a) == TASK_OK && out == a);
    assert(tasks[1].wait_result == TASK_OK && queue.count == 0);
    handler_context = false;
    assert(queue_send_isr(&queue, &a) == TASK_ERR_CONTEXT);
    wait_on(0, &queue, TASK_WAIT_RECEIVE, &out, 1);
    tick();
    out = 0;
    assert(queue_send(&queue, &b, 0) == TASK_OK);
    assert(out == 0 && queue.count == 1);
}

static void test_semaphore(void)
{
    reset(3);
    semaphore_t semaphore;
    assert(semaphore_init(&semaphore, 0, 1) == TASK_OK);
    assert(semaphore_take(&semaphore, 0) == TASK_ERR_TIMEOUT);
    wait_on(1, &semaphore, TASK_WAIT_SEMAPHORE, NULL, TASK_WAIT_FOREVER);
    wait_on(0, &semaphore, TASK_WAIT_SEMAPHORE, NULL, TASK_WAIT_FOREVER);
    tasks[2].base_priority = 7;
    wait_on(2, &semaphore, TASK_WAIT_SEMAPHORE, NULL, TASK_WAIT_FOREVER);
    assert(semaphore_give(&semaphore) == TASK_OK);
    assert(tasks[2].state == TASK_READY);
    assert(semaphore_give(&semaphore) == TASK_OK);
    assert(tasks[1].state == TASK_READY);
    handler_context = true;
    assert(semaphore_give(&semaphore) == TASK_ERR_CONTEXT);
    assert(semaphore_give_isr(&semaphore) == TASK_OK);
    handler_context = false;
    assert(semaphore_give(&semaphore) == TASK_OK);
    assert(semaphore_give(&semaphore) == TASK_ERR_LIMIT);
    assert(semaphore_take(&semaphore, 0) == TASK_OK);
}

static void test_mutex(void)
{
    reset(3);
    mutex_t first, second;
    assert(mutex_init(&first) == TASK_OK);
    assert(mutex_init(&second) == TASK_OK);
    tasks[0].base_priority = 1;
    tasks[1].base_priority = 4;
    tasks[2].base_priority = 7;
    scheduler_current = &tasks[0];
    assert(mutex_lock(&first, 0) == TASK_OK);
    assert(mutex_lock(&first, 0) == TASK_ERR_DEADLOCK);
    scheduler_current = &tasks[1];
    assert(mutex_unlock(&first) == TASK_ERR_OWNERSHIP);
    assert(mutex_lock(&second, 0) == TASK_OK);
    wait_on(1, &first, TASK_WAIT_MUTEX, NULL, TASK_WAIT_FOREVER);
    wait_on(2, &second, TASK_WAIT_MUTEX, NULL, 2);
    assert(tasks[0].priority == 7 && tasks[1].priority == 7);
    scheduler_current = &tasks[0];
    assert(mutex_lock(&second, 5) == TASK_ERR_DEADLOCK);
    tick(); tick();
    assert(tasks[2].wait_result == TASK_ERR_TIMEOUT);
    assert(tasks[0].priority == 4 && tasks[1].priority == 4);
    scheduler_current = &tasks[0];
    assert(mutex_unlock(&first) == TASK_OK);
    assert(first.owner == &tasks[1] && tasks[1].state == TASK_READY);
    assert(tasks[0].priority == 1);
    wait_on(2, &first, TASK_WAIT_MUTEX, NULL, TASK_WAIT_FOREVER);
    assert(tasks[1].priority == 7);
    scheduler_current = &tasks[1];
    assert(mutex_unlock(&second) == TASK_OK);
    assert(tasks[1].priority == 7);
    assert(mutex_unlock(&first) == TASK_OK);
    assert(tasks[1].priority == 4 && first.owner == &tasks[2]);
}

static void resume_with_timeout(void) { tick(); tick(); }
static void test_block_result(void)
{
    reset(1);
    semaphore_t semaphore;
    assert(semaphore_init(&semaphore, 0, 1) == TASK_OK);
    select_next();
    resume_hook = resume_with_timeout;
    assert(semaphore_take(&semaphore, 2) == TASK_ERR_TIMEOUT);
    assert(tasks[0].state == TASK_READY);
}

static void test_stack(void)
{
    reset(1);
    size_t words;
    assert(task_stack_usage(0, &words) == TASK_OK && words == 16);
    stacks[0][80] = 123;
    assert(task_stack_usage(0, &words) == TASK_OK && words == 48);
    stacks[0][0] = 0;
    assert(task_stack_usage(0, &words) == TASK_ERR_STATE);
    if (setjmp(panic_target) == 0) {
        select_next();
        assert(false);
    }
    assert(observed_panic == KERNEL_PANIC_STACK);
}

int main(void)
{
    test_creation();
    test_scheduling();
    test_time_and_context();
    test_queue();
    test_semaphore();
    test_mutex();
    test_block_result();
    test_stack();
    puts("PASS: creation, scheduling, wraparound, critical sections, queues, semaphores, mutexes, stacks");
}
