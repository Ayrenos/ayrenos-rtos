.syntax unified
.cpu cortex-m3
.thumb

.section .text.task_port_start_first, "ax", %progbits
.global task_port_start_first
.type task_port_start_first, %function
.thumb_func

task_port_start_first:
    svc #0

.Lunexpected_return:
    b .Lunexpected_return

.size task_port_start_first, .-task_port_start_first


.section .text.SVC_Handler, "ax", %progbits
.global SVC_Handler
.type SVC_Handler, %function
.thumb_func

SVC_Handler:
    push {r0, lr}
    bl task_port_start_tick
    pop {r0, lr}

    ldr r0, =scheduler_current
    ldr r0, [r0]

    ldr r0, [r0]

    ldmia r0!, {r4-r11}

    msr psp, r0

    movs r0, #2
    msr control, r0
    isb

    ldr r0, =_estack
    msr msp, r0

    ldr lr, =0xFFFFFFFD
    bx lr

.size SVC_Handler, .-SVC_Handler

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
