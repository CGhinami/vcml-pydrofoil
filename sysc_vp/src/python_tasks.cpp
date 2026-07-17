/******************************************************************************
 *                                                                            *
 * Copyright 2026 Chiara Ghinami                                              *
 *                                                                            *
 * This software is licensed under the MIT license found in the               *
 * LICENSE file at the root directory of this source tree.                    *
 *                                                                            *
 ******************************************************************************/

#include "python_tasks.h"
#include "core.h"

namespace backend {

auto create_handlers(core::PydrofoilCore& pycore) -> std::unordered_map<Funct, std::function<void(PythonTask&)>>
{
    return {{Funct::Init,
             [&pycore](PythonTask& task) { // the lambda should keep a referece of PydrofoilCore
#if PROFILING
                 Profiler t("Init");
#endif
                 auto core_type = std::get<std::string>(task.arg);
                 pycore.cpu = pydrofoil_allocate_cpu(core_type.data(), nullptr);
                 task.result.set_value(0);
             }},
            {Funct::SetCb,
             [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("SetCb");
#endif
                 int res = pydrofoil_cpu_set_ram_read_write_callback(pycore.cpu, read_mem, write_mem, &pycore); //
                 task.result.set_value(res);
             }},
            {Funct::GetCycles,
             [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("GetCycles");
#endif
                 pycore.n_cycles = pydrofoil_cpu_cycles(pycore.cpu);
                 task.result.set_value(pycore.n_cycles);
             }},
            {Funct::SetBrkp,
             [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("SetBrkp");
#endif
                 auto addr = std::get<size_t>(task.arg);
                 int res = pydrofoil_cpu_set_breakpoint(pycore.cpu, addr);
                 task.result.set_value(int(res == 0));
             }},
            {Funct::RemoveBrkp,
             [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("RemoveBrkp");
#endif
                 auto addr = std::get<size_t>(task.arg);
                 int res = pydrofoil_cpu_remove_breakpoint(pycore.cpu, addr);
                 task.result.set_value(int(res == 0));
             }},
            {Funct::Simulate,
             [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("Simulate");
#endif
                 auto cycles = std::get<size_t>(task.arg);
                 auto n_steps = pydrofoil_cpu_simulate(pycore.cpu, cycles);
                 // pycore.n_cycles = pydrofoil_cpu_cycles(pycore.cpu);
                 task.result.set_value(n_steps);
                 pycore.memtask_cv.notify_one();
             }},
            {Funct::WriteReg,
             [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("WriteReg");
#endif
                 auto args = std::get<WriteRegArgs>(task.arg);
                 int res = pydrofoil_cpu_write_reg(pycore.cpu, args.reg_name, args.value);
                 task.result.set_value(int(res == 0));
             }},
            {Funct::ReadReg,
             [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("ReadReg");
#endif
                 auto reg_name = std::get<std::string>(task.arg);
                 auto reg_value = pydrofoil_cpu_read_reg(pycore.cpu, reg_name.c_str());
                 task.result.set_value(reg_value);
             }},
            {Funct::FreeCpu,
             [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("FreeCpu");
#endif
                 pydrofoil_free_cpu(pycore.cpu);
                 task.result.set_value(0);
             }},
            {Funct::SetVerbosity,
             [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("SetVerbosity");
#endif
                 auto verbosity = std::get<uint32_t>(task.arg);
                 pydrofoil_cpu_set_verbosity(pycore.cpu, verbosity);
                 task.result.set_value(0);
             }},
            {Funct::SetDMI,
             [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("SetDMI");
#endif
                 auto start_addr = std::get<size_t>(task.arg);
                 auto dmi_region = pycore.mem_regions[start_addr];
                 int res = pydrofoil_cpu_set_dma_region(pycore.cpu, start_addr, dmi_region.size, dmi_region.ptr);
                 task.result.set_value(res);
             }},
            {Funct::SetMIP, [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("RaiseIrq");
#endif
                 auto value = std::get<size_t>(task.arg);
                 pydrofoil_set_interrupt_pending(pycore.cpu, value);
                 task.result.set_value(0);
             }}};
}

} // namespace backend
