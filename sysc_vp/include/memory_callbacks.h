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

// Executes one RISC-V A-extension instruction on behalf of the guest.
//
// Every hart runs its own dlmopen'd copy of libpydrofoilcapi_cffi.so, so each
// one has a private Sail model with a private LR/SC reservation and a private
// view of RAM through DMI. A reservation that only one hart can see is useless:
// two harts leave the same critical section believing they own the lock, and
// the scheduler lists they protect end up half-unlinked. All those copies do
// share this VP binary, which is why the reservation state and the
// read-modify-write live here and not behind the CFFI boundary.
//
// Returns 0 when the instruction was executed and 'result' holds the value for
// rd, non-zero when the caller has to fall back to the Sail model.
int atomic_mem(void* cpu, uint32_t insn, uint64_t address, uint64_t src, uint64_t* result, void* payload);
}
 
 #endif