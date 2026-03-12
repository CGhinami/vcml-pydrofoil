#pragma once
#include "arch.h"
#include <string.h>

static const Reg regdb_riscv[] = {
    {"zero", "x0"}, // I dont know why accessing this is impossible...
    {"ra", "x1"},   // Return address
    {"sp", "x2"},   // Stack Pointer
    {"gp", "x3"},   // Global pointer
    {"tp", "x4"},   // Thread pointer
    {"t0", "x5"},   // Temporary Register (caller-saved)
    {"t1", "x6"},   
    {"t2", "x7"},
    {"s0", "x8"},   // Saved register (callee-saved)
    {"s1", "x9"},
    {"a0", "x10"},  // Argument register (used for return values as well)
    {"a1", "x11"},
    {"a2", "x12"},
    {"a3", "x13"},
    {"a4", "x14"},
    {"a5", "x15"},
    {"a6", "x16"},
    {"a7", "x17"},
    {"s2", "x18"},  // Saved Register
    {"s3", "x19"},
    {"s4", "x20"},
    {"s5", "x21"},
    {"s6", "x22"},
    {"s7", "x23"},
    {"s8", "x24"},
    {"s9", "x25"},
    {"s10", "x26"},
    {"s11", "x27"},
    {"t3", "x28"},  // Temporary register (caller-saved)
    {"t4", "x29"},
    {"t5", "x30"},
    {"t6", "x31"},
    {"pc", "pc"}
};