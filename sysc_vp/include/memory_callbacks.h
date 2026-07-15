/******************************************************************************
 *                                                                            *
 * Copyright 2026 Chiara Ghinami                                              *
 *                                                                            *
 * This software is licensed under the MIT license found in the               *
 * LICENSE file at the root directory of this source tree.                    *
 *                                                                            *
 ******************************************************************************/

#ifndef PYTHON_CALLBACKS_H
#define PYTHON_CALLBACKS_H

#include <cstdint>

extern "C" {
int read_mem(void* cpu, uint64_t address, int size, void* destination, void* payload);
int write_mem(void* cpu, uint64_t address, int size, uint64_t value, void* payload);
}

#endif
