#pragma once
#include <unordered_map>
#include "memory_callbacks.h"
#include <future>
#include <variant>
#include "profiling.h"

extern "C" {
    #include "pydrofoilcapi.h" 
}

// std::monostate allows us to have no argument (and still have a valid arg which will default to monostate)
using TaskArg = std::variant<std::monostate, size_t, const char*>;
// enum class: no implicit conversion, name's scoped to enum
enum class Funct {Init, SetCb, Simulate, GetCycles, SetPc, ReadPc, FreeCpu, SetVerbosity, SetDMI, SetMIP};

struct PythonTask {
    Funct py_funct;
    TaskArg arg;
    std::promise<uint64_t> result; // Avoids the burden of sync threads
};  

class PydrofoilCore; // The compiler needs to know that PydrofoilCore is a class

auto create_handlers(PydrofoilCore& core)
    -> std::unordered_map<Funct, std::function<void(PythonTask&)>>;

