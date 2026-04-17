#ifndef PYTHON_CALLBACKS_H
#define PYTHON_CALLBACKS_H

#include <cstdint>
#include "core.h"  // for PydrofoilCore

extern "C" {
    int read_mem(void* cpu, uint64_t address, int size, void* destination, void* payload);
    int write_mem(void* cpu, uint64_t address, int size, uint64_t value, void* payload);
}

#endif