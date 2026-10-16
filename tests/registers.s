.syntax unified
.cpu cortex-m3
.thumb

.section .text.test_registers, "ax", %progbits
.global test_registers
.type test_registers, %function
.thumb_func

test_registers:
    push {r4-r11, lr}
    sub sp, #4
    mov.w r4, #0x44
    mov.w r5, #0x55
    mov.w r6, #0x66
    mov.w r7, #0x77
    mov.w r8, #0x88
    mov.w r9, #0x99
    mov.w r10, #0xAA
    mov.w r11, #0xBB

    bl task_yield
    cmp r0, #0
    bne .Lbad_register
    cmp r4, #0x44
    bne .Lbad_register
    cmp r5, #0x55
    bne .Lbad_register
    cmp r6, #0x66
    bne .Lbad_register
    cmp r7, #0x77
    bne .Lbad_register
    cmp r8, #0x88
    bne .Lbad_register
    cmp r9, #0x99
    bne .Lbad_register
    cmp r10, #0xAA
    bne .Lbad_register
    cmp r11, #0xBB
    bne .Lbad_register
    movs r0, #1
    b .Lregister_return

.Lbad_register:
    movs r0, #0

.Lregister_return:
    add sp, #4
    pop {r4-r11, pc}

.size test_registers, .-test_registers
