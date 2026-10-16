# ayrenos-rtos

A small, statically allocated Cortex-M3 RTOS for the STM32 Nucleo-F103RB.
The kernel supports eight application tasks plus its own idle task. It uses
caller-provided stacks and storage, without a heap or a C runtime library.

The example toggles PA5 every 500 ticks. A second task sends a heartbeat every
125 ticks; the LED task drains those messages. Inspect `worker_counters` and
`last_heartbeat` in the debugger to see independent progress.

## Build and run

Put `arm-none-eabi-gcc` and CMake on your PATH, then run:

```sh
cmake -S . -B build
cmake --build build
arm-none-eabi-size build/ayrenos.elf
```

Load `build/ayrenos.elf` with an ST-LINK debugger or STM32CubeProgrammer, then
reset and run the board. The linker reserves 128 KB of flash, 20 KB of RAM,
and 1 KB at the top of RAM for the main/interrupt stack. The build also writes
`build/ayrenos.map` to help inspect memory use.

`BOARD_CORE_CLOCK_HZ` assumes the reset clock: 8 MHz HSI with no PLL or divider.
`KERNEL_TICK_HZ` is 1000, so one tick is one millisecond on this board. If you
change the hardware clock, update the declared frequency too. Clock changes
after kernel startup are unsupported.

## Tasks and scheduling

Create tasks before `kernel_start()`. Stacks must be eight-byte aligned, have
an even number of words, contain at least 64 words, and remain alive. Priorities
range from 1 to 7; larger values run first. Creation is sealed at startup.

```c
_Alignas(8) static uint32_t stack[256];

static void worker(void *argument)
{
    (void)argument;

    for (;;) {
        /* Do a bounded amount of work. */
        (void)task_sleep(100);
    }
}

const task_config_t config = {
    .entry = worker,
    .argument = NULL,
    .stack = stack,
    .stack_words = sizeof(stack) / sizeof(stack[0]),
    .priority = 2
};

task_id_t id;
task_result_t result = task_create(&config, &id);
/* Check result before calling kernel_start(). */
```

The highest-priority ready task runs. Equal-priority tasks rotate on each tick
and on rescheduling events. `task_yield()` lets another equal-priority task run;
a ready higher-priority task still takes precedence. `task_sleep(0)` yields.
Finite sleeps support 1 through `TASK_WAIT_MAX` ticks and handle tick wraparound.
The internal priority-zero idle task uses `WFI` when no application is ready.

SysTick updates time and requests PendSV. PendSV saves r4-r11 and the outgoing
PSP; exception entry/return saves and restores the remaining hardware frame.
SVC starts the first task. Application tasks run privileged on PSP, while
interrupt handlers use MSP. Tasks must run indefinitely: returning from a task
triggers a panic. Task deletion, suspension, and runtime creation are unsupported.

## Synchronization

Include `kernel/sync.h`. Initialize every object once, before kernel startup.
Objects and queue storage must remain alive and must not be modified directly
while the kernel runs. A queue copies fixed-size items into caller-owned storage.
All application callers should check operation results.

| Operation | Behavior |
| --- | --- |
| `queue_send` / `queue_receive` | Copy an item, or wait for space/data. |
| `semaphore_take` / `semaphore_give` | Take a counting token, or give one to a waiting task. |
| `mutex_lock` / `mutex_unlock` | Acquire/release a nonrecursive, task-owned lock. |

For operations accepting a timeout, zero polls, `TASK_WAIT_FOREVER` waits
indefinitely, and 1 through `TASK_WAIT_MAX` are finite tick counts. Unavailable
polls and expired waits return `TASK_ERR_TIMEOUT`. A semaphore at its maximum
returns `TASK_ERR_LIMIT` when given without a waiter.

Waiters are selected by effective priority, then arrival order. Queue wakeups
complete the item transfer before waking the task; semaphore wakeups transfer
the token directly. Mutex unlock transfers ownership directly to a waiter.
Mutexes inherit waiting tasks' priorities, including through ownership chains
and across multiple held mutexes. Donations are removed on timeout or unlock.
Recursive locking and mutex ownership cycles return `TASK_ERR_DEADLOCK`;
unlocking another task's mutex returns `TASK_ERR_OWNERSHIP`.

## Interrupts and critical sections

`kernel_critical_enter()` returns the previous PRIMASK. Pass that exact value
to `kernel_critical_exit()`; nested sections preserve the original mask. Keep
critical sections short because interrupts, including ticks, are deferred.
Large queue items also increase interrupt latency while they are copied.

Sleep, yield, mutex operations, and nonzero-timeout waits reject interrupt
context or a masked-interrupt thread context. Polling queue/semaphore operations
may run in a thread's critical section. Ordinary interrupt handlers may only
use `queue_send_isr`, `queue_receive_isr`, and `semaphore_give_isr` for
synchronization; those never block and request a switch after waking a task.
These ISR APIs must not be called from NMI or fault handlers. Kernel critical
sections cannot mask those exceptions.

## Diagnostics

`task_stack_usage(id, &words)` estimates the stack high-water mark by scanning
the fill pattern. It includes the initial context and excludes the canary word;
matching application data can make the estimate undercount. PendSV checks stack
bounds before saving registers and checks the bottom canary at every switch.
These checks detect errors; they do not provide memory isolation or prevent
all writes from an overflowing stack. Account separately for the MSP budget.

Set a breakpoint in `kernel_panic()` and inspect `kernel_panic_reason`.
HardFault recording exposes `kernel_fault`, including CFSR/HFSR, fault addresses,
EXC_RETURN, and the stacked registers when the frame is valid. Consult the CFSR
validity bits before interpreting MMFAR/BFAR. Fault capture and panic halt the
CPU until reset; there is no recovery or persistent fault log.

The Cortex-M3 register configuration follows the
[Arm CMSIS core definitions](https://github.com/ARM-software/CMSIS_5/blob/develop/CMSIS/Core/Include/core_cm3.h).

## Tests

```sh
python3 tests/run.py
# Only host boundary tests:
python3 tests/run.py --host-only
```

The host tests require a C11 compiler and enable undefined-behavior checking.
They cover task validation, priority/round-robin selection, idle fallback,
tick wraparound, nested critical sections, queue handoff, semaphore ordering,
timeout cleanup, chained priority inheritance, deadlock checks, and stack checks.

The full suite also requires `arm-none-eabi-gcc` and `qemu-system-arm`. It runs
actual Cortex-M3 startup and context-switch assembly on QEMU's MPS2-AN385 at
`-O0` and `-O2`, testing CPU-bound preemption, r4-r11 preservation, sleep,
blocking synchronization, timeouts, priority inheritance, and stack usage.
Only the test flash address is changed for MPS2; the STM32 GPIO driver is not
used. This validates the CPU port, not the physical Nucleo's clock, GPIO, or
ST-LINK connection. Physical-board verification is still needed.

`CC` selects the host compiler and `HOST_CFLAGS` supplies extra host flags.
For an older Apple compiler with an incompatible default SDK, select a matching
installed SDK, for example:

```sh
HOST_CFLAGS='-isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX13.1.sdk' python3 tests/run.py
```
