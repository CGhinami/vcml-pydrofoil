    .section .text
    .globl _start
    .globl trap_entry

_start:
    # set up stack pointer
    la sp, _stack_top

    # set trap vector to trap_entry
    # Function the execution jumps to when an interrupt is pending
    la t0, trap_entry
    csrw mtvec, t0

    j main

trap_entry:

    addi sp, sp, -64

    # store registers that will be changed
    # ra: return address
    # t0-t2: scratch registers
    sw ra, 0(sp)
    sw t0, 4(sp)
    sw t1, 8(sp)
    sw t2, 12(sp)

    call trap_dispatch

    # restore registers
    lw ra, 0(sp)
    lw t0, 4(sp)
    lw t1, 8(sp)
    lw t2, 12(sp)

    addi sp, sp, 64

    # return from interrupt
    # sometimes the compiler adds this automatically when seeing that 
    # this function has a interrupt attribute (__attribute__((interrupt)))
    mret
