    .section .text

    .globl _start
    .type _start, @function

    .globl trap_entry
    .type trap_entry, @function

_start:
    # set up stack pointer
    la sp, _stack_top

    # set trap vector to trap_entry
    # Function the execution jumps to when an interrupt is pending
    la t0, trap_entry
    csrw mtvec, t0

    j main
.size _start, . - _start

trap_entry:
    addi sp, sp, -64

    sw ra, 0(sp)
    sw t0, 4(sp)
    sw t1, 8(sp)
    sw t2, 12(sp)

    sw s0, 16(sp)
    sw s1, 20(sp)
    sw s2, 24(sp)
    sw s3, 28(sp)
    sw s4, 32(sp)
    sw s5, 36(sp)
    sw s6, 40(sp)
    sw s7, 44(sp)
    sw s8, 48(sp)
    sw s9, 52(sp)
    sw s10, 56(sp)
    sw s11, 60(sp)

    call trap_dispatch

    lw ra, 0(sp)
    lw t0, 4(sp)
    lw t1, 8(sp)
    lw t2, 12(sp)

    lw s0, 16(sp)
    lw s1, 20(sp)
    lw s2, 24(sp)
    lw s3, 28(sp)
    lw s4, 32(sp)
    lw s5, 36(sp)
    lw s6, 40(sp)
    lw s7, 44(sp)
    lw s8, 48(sp)
    lw s9, 52(sp)
    lw s10, 56(sp)
    lw s11, 60(sp)

    addi sp, sp, 64

    # return from interrupt
    # sometimes the compiler adds this automatically when seeing that 
    # this function has a interrupt attribute (__attribute__((interrupt)))
    mret

.size trap_entry, . - trap_entry
