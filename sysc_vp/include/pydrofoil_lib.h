/******************************************************************************
 *                                                                            *
 * Copyright 2026 Luis Seibt                                                  *
 *                                                                            *
 * This software is licensed under the MIT license found in the               *
 * LICENSE file at the root directory of this source tree.                    *
 *                                                                            *
 ******************************************************************************/

#ifndef PYDROFOIL_LIB_H
#define PYDROFOIL_LIB_H

#include <string>

extern "C" {
#include "pydrofoilcapi.h"
}

namespace backend {

// Every function of pydrofoilcapi.h the VP calls
#define PYDROFOIL_API(X)                        \
    X(pydrofoil_allocate_cpu)                   \
    X(pydrofoil_free_cpu)                       \
    X(pydrofoil_cpu_simulate)                   \
    X(pydrofoil_cpu_cycles)                     \
    X(pydrofoil_cpu_set_verbosity)              \
    X(pydrofoil_cpu_read_reg)                   \
    X(pydrofoil_cpu_write_reg)                  \
    X(pydrofoil_cpu_set_breakpoint)             \
    X(pydrofoil_cpu_remove_breakpoint)          \
    X(pydrofoil_get_htif_done)                  \
    X(pydrofoil_cpu_set_htif_tohost)            \
    X(pydrofoil_cpu_set_external_clint)         \
    X(pydrofoil_set_interrupt_lines)            \
    X(pydrofoil_set_instructions_per_tick)      \
    X(pydrofoil_cpu_set_hartid)                 \
    X(pydrofoil_cpu_set_ram_read_write_callback) \
    X(pydrofoil_cpu_set_dma_region)

// One private copy of libpydrofoilcapi_cffi.so (and with it of the whole PyPy
// runtime) per hart. dlmopen() loads it into a new link-map namespace, so the
// harts share no interpreter, no GIL and no gluecode globals and can run in
// parallel threads (async=true).
struct PydrofoilLib {
    void* handle = nullptr;

#define PYDROFOIL_LIB_FN(fn) decltype(&::fn) fn = nullptr;
    PYDROFOIL_API(PYDROFOIL_LIB_FN)
#undef PYDROFOIL_LIB_FN

    PydrofoilLib() = default;
    PydrofoilLib(const PydrofoilLib&) = delete;
    PydrofoilLib& operator=(const PydrofoilLib&) = delete;
    ~PydrofoilLib();

    // Returns an error message, empty on success
    std::string load(const std::string& path);
};

} // namespace backend

#endif
