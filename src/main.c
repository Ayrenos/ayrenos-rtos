#include "board.h"
#include "uart.h"
#include "kernel/task.h"
#include "kernel/scheduler.h"
#include "kernel/sync.h"

_Alignas(8) static uint32_t worker_stacks[2][256];

static uint32_t periods[2] = { 500U, 125U };
static queue_t messages;
static uint32_t message_storage[4];

/* Inspect these in the debugger to check startup and independent task progress. */
volatile task_result_t boot_task_result;
volatile uint32_t worker_counters[2];
volatile uint32_t last_heartbeat;

static void blink(void *argument)
{
    const uint32_t period = *(const uint32_t *)argument;
    bool on = false;

    for (;;) {
        on = !on;
        board_led_write(on);
        ++worker_counters[0];

        uint32_t value;

        while (queue_receive(&messages, &value, 0) == TASK_OK) {
            last_heartbeat = value;
        }

        (void)task_sleep(period);
    }
}

static void heartbeat(void *argument)
{
    const uint32_t period = *(const uint32_t *)argument;

    for (;;) {
        uint32_t value = ++worker_counters[1];
        (void)queue_send(&messages, &value, 0);
        (void)task_sleep(period);
    }
}

int main(void)
{
    board_init();
    uart_init();
    uart_write("AyrenOS starting\r\n");
    boot_task_result = queue_init(&messages, message_storage, 4, sizeof(uint32_t));

    for (size_t i = 0; i < 2U && boot_task_result == TASK_OK; ++i) {
        task_id_t id;
        const task_config_t config = {
            .entry = i == 0U ? blink : heartbeat,
            .argument = &periods[i],
            .stack = worker_stacks[i],
            .stack_words = sizeof(worker_stacks[i]) / sizeof(worker_stacks[i][0]),
            .priority = 2
        };

        boot_task_result = task_create(&config, &id);
    }

    if (boot_task_result != TASK_OK) {
        for (;;) {
            __asm volatile ("nop");
        }
    }

    kernel_start();
}
