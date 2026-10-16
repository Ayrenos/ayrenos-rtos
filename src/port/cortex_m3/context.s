.syntax unified
.cpu cortex-m3
.thumb

/* Called once by kernel_start(). */
.section .text.task_port_start_first, "ax", %progbits
.global task_port_start_first
.type task_port_start_first, %function
.thumb_func

task_port_start_first:
    svc #0

/* Successful startup never returns here. */
.Lunexpected_return:
    b .Lunexpected_return

.size task_port_start_first, .-task_port_start_first


/* Startup-only SVC handler. */
.section .text.SVC_Handler, "ax", %progbits
.global SVC_Handler
.type SVC_Handler, %function
.thumb_func

SVC_Handler:
    /* SVC has higher priority than SysTick and PendSV during startup. */
    push {r0, lr}
    bl task_port_start_tick
    pop {r0, lr}

    /* r0 = scheduler_current */
    ldr r0, =scheduler_current
    ldr r0, [r0]

    /* r0 = scheduler_current->sp */
    ldr r0, [r0]

    /* Restore the eight software-saved registers. */
    ldmia r0!, {r4-r11}

    /* r0 now points to the hardware frame, starting at r0. */
    msr psp, r0

    /* Privileged thread mode, using PSP. */
    movs r0, #2
    msr control, r0
    isb

    /* Discard the startup call stack; main never resumes. */
    ldr r0, =_estack
    msr msp, r0

    /* Exception return to thread mode using PSP. */
    ldr lr, =0xFFFFFFFD
    bx lr

.size SVC_Handler, .-SVC_Handler

/* Save r4-r11 in software; exception entry has already saved r0-r3 and lr. */
.section .text.PendSV_Handler, "ax", %progbits
.global PendSV_Handler
.type PendSV_Handler, %function
.thumb_func

PendSV_Handler:
    mrs r3, primask
    cpsid i
    mrs r0, psp
    ldr r1, =scheduler_current
    ldr r2, [r1]

    /* Check bounds before writing the software frame below PSP. */
    ldr r1, [r2, #4]
    adds r1, #36
    cmp r0, r1
    blo .Lstack_error
    ldr r1, [r2, #4]
    ldr r12, [r2, #8]
    add r1, r1, r12, lsl #2
    subs r1, #32
    cmp r0, r1
    bhi .Lstack_error
    tst r0, #7
    bne .Lstack_error

    stmdb r0!, {r4-r11}
    str r0, [r2]

    /* Keep MSP eight-byte aligned across the C call. */
    push {r3, lr}
    bl scheduler_select
    pop {r3, lr}

    ldr r1, =scheduler_current
    ldr r2, [r1]
    ldr r0, [r2]
    ldmia r0!, {r4-r11}
    msr psp, r0
    msr primask, r3
    isb
    bx lr

.Lstack_error:
    movs r0, #2
    b kernel_panic

.size PendSV_Handler, .-PendSV_Handler


/* Forward the hardware exception frame and EXC_RETURN to the fault recorder. */
.section .text.HardFault_Handler, "ax", %progbits
.global HardFault_Handler
.type HardFault_Handler, %function
.thumb_func

HardFault_Handler:
    tst lr, #4
    ite eq
    mrseq r0, msp
    mrsne r0, psp
    mov r1, lr
    b kernel_hardfault

.size HardFault_Handler, .-HardFault_Handler
