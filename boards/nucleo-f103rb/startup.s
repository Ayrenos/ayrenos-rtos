/* Select assembly syntac and CPU instruction set */
.syntax unified
.cpu cortex-m3
.thumb

/* puts vector table in section the linker script collects */
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

    .word Default_Handler  /* SVC */
    .word Default_Handler  /* Debug monitor */
    .word 0                /* Reserved */
    .word Default_Handler  /* PendSV */
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