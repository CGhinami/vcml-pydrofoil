/******************************************************************************
 *                                                                            *
 * Copyright 2026 Chiara Ghinami                                              *
 *                                                                            *
 * This software is licensed under the MIT license found in the               *
 * LICENSE file at the root directory of this source tree.                    *
 *                                                                            *
 ******************************************************************************/

#pragma once
#include <unordered_map>
#include "memory_callbacks.h"
#include <future>
#include <variant>
#include "profiling.h"

extern "C" {
#include "pydrofoilcapi.h"
}

// Forward declaration of the core class
namespace core {
class PydrofoilCore;
}

namespace backend {

struct WriteRegArgs {
    const char* reg_name;
    size_t value;
};

// std::monostate allows us to have no argument (and still have a valid arg which will default to monostate)
using TaskArg = std::variant<std::monostate, uint64_t, uint32_t, std::string, WriteRegArgs>;
// enum class: no implicit conversion, name's scoped to enum
enum class Funct {
    Init,
    SetCb,
    Simulate,
    GetCycles,
    WriteReg,
    ReadReg,
    FreeCpu,
    SetVerbosity,
    SetDMI,
    SetMIP,
    SetBrkp,
    RemoveBrkp
};

struct PythonTask {
    Funct py_funct;
    TaskArg arg;
    std::promise<uint64_t> result; // Avoids the burden of sync threads
};

auto create_handlers(core::PydrofoilCore& pycore) -> std::unordered_map<Funct, std::function<void(PythonTask&)>>;

} // namespace backend
