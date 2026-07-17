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
    addi sp, sp, -128

    sd ra, 0(sp)
    sd t0, 8(sp)
    sd t1, 16(sp)
    sd t2, 24(sp)

    sd s0, 32(sp)
    sd s1, 40(sp)
    sd s2, 48(sp)
    sd s3, 56(sp)
    sd s4, 64(sp)
    sd s5, 72(sp)
    sd s6, 80(sp)
    sd s7, 88(sp)
    sd s8, 96(sp)
    sd s9, 104(sp)
    sd s10, 112(sp)
    sd s11, 120(sp)

    call trap_dispatch

    ld ra, 0(sp)
    ld t0, 8(sp)
    ld t1, 16(sp)
    ld t2, 24(sp)

    ld s0, 32(sp)
    ld s1, 40(sp)
    ld s2, 48(sp)
    ld s3, 56(sp)
    ld s4, 64(sp)
    ld s5, 72(sp)
    ld s6, 80(sp)
    ld s7, 88(sp)
    ld s8, 96(sp)
    ld s9, 104(sp)
    ld s10, 112(sp)
    ld s11, 120(sp)

    addi sp, sp, 128

    # return from interrupt
    # sometimes the compiler adds this automatically when seeing that 
    # this function has a interrupt attribute (__attribute__((interrupt)))
    mret

.size trap_entry, . - trap_entry
