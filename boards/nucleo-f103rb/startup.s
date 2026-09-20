/* Select assembly syntac and CPU instruction set */
.syntax unified
.cpu cortex-m3
.thumb

/* Puts vector table in section the linker script collects */
.section .isr_vector, "a"
.balign 256

.global vector_table
vector_table:

    .word _estack          /* Initial mains tack pointer (MSB) */
    .word Reset_Handler    /* Code to execute after reset */

    .word Default_Handler  /* NMI */
    .word Default_Handler  /* HardFault */
    .word Default_Handler  /* MemManage */
    .word Default_Handler  /* BusFault */
    .word Default_Handler  /* UsageFault */

    .word 0                /* Reserved */
    .word 0                /* Reserved */
    .word 0                /* Reserved */
    .word 0                /* Reserved */

    .word SVC_Handler      /* SVC */
    .word Default_Handler  /* Debug monitor */
    .word 0                /* Reserved */
    .word PendSV_Handler   /* PendSV */
    .word Default_Handler  /* SysTick */

    /* Gives all 43 peripheral interrupts a default handler */
    .rept 43
        .word Default_Handler
    .endr

/* Stop in section if unexpected interrupt or fault occurs */
.section .text.Default_Handler, "ax"
.thumb_func
Default_Handler:
    b Default_Handler

.section .text.Reset_Handler, "ax"
.global Reset_Handler
.type Reset_Handler, %function
.thumb_func

Reset_Handler:
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata

.Lcopy_data:
    cmp r1, r2
    bhs .Lclear_bss_setup

    ldr r3, [r0], #4
    str r3, [r1], #4
    b .Lcopy_data

.Lclear_bss_setup:
    ldr r0, =_sbss
    ldr r1, =_ebss
    movs r2, #0

.Lclear_bss:
    cmp r0, r1
    bhs .Lstart_main

    str r2, [r0], #4
    b .Lclear_bss

.Lstart_main:
    bl main
    b Default_Handler

.size Reset_Handler, .-Reset_Handler