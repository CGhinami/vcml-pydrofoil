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
                 pycore.cpu = pycore.lib.pydrofoil_allocate_cpu(core_type.data(), nullptr);
                 task.result = pycore.cpu != nullptr ? 0 : 1;
             }},
            {Funct::SetCb,
             [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("SetCb");
#endif
                 int res = pycore.lib.pydrofoil_cpu_set_ram_read_write_callback(pycore.cpu, read_mem, write_mem, &pycore); //
                 task.result = res;
             }},
            {Funct::GetCycles,
             [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("GetCycles");
#endif
                 pycore.n_cycles = pycore.lib.pydrofoil_cpu_cycles(pycore.cpu);
                 task.result = pycore.n_cycles;
             }},
            {Funct::SetBrkp,
             [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("SetBrkp");
#endif
                 auto addr = std::get<size_t>(task.arg);
                 int res = pycore.lib.pydrofoil_cpu_set_breakpoint(pycore.cpu, addr);
                 task.result = int(res == 0);
             }},
            {Funct::RemoveBrkp,
             [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("RemoveBrkp");
#endif
                 auto addr = std::get<size_t>(task.arg);
                 int res = pycore.lib.pydrofoil_cpu_remove_breakpoint(pycore.cpu, addr);
                 task.result = int(res == 0);
             }},
            {Funct::Simulate,
             [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("Simulate");
#endif
                 auto cycles = std::get<size_t>(task.arg);
                 auto n_steps = pycore.lib.pydrofoil_cpu_simulate(pycore.cpu, cycles);
                 // pycore.n_cycles = pycore.lib.pydrofoil_cpu_cycles(pycore.cpu);
                 task.result = n_steps;
                 // no notify needed any more: the SystemC thread is spinning on
                 // task_mailbox.is_done() in simulate(), not blocked on a condition_variable
             }},
            {Funct::WriteReg,
             [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("WriteReg");
#endif
                 auto args = std::get<WriteRegArgs>(task.arg);
                 int res = pycore.lib.pydrofoil_cpu_write_reg(pycore.cpu, args.reg_name, args.value);
                 task.result = int(res == 0);
             }},
            {Funct::ReadReg,
             [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("ReadReg");
#endif
                 auto reg_name = std::get<std::string>(task.arg);
                 auto reg_value = pycore.lib.pydrofoil_cpu_read_reg(pycore.cpu, reg_name.c_str());
                 task.result = reg_value;
             }},
            {Funct::FreeCpu,
             [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("FreeCpu");
#endif
                 pycore.lib.pydrofoil_free_cpu(pycore.cpu);
                 task.result = 0;
             }},
            {Funct::SetVerbosity,
             [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("SetVerbosity");
#endif
                 auto verbosity = std::get<uint32_t>(task.arg);
                 pycore.lib.pydrofoil_cpu_set_verbosity(pycore.cpu, verbosity);
                 task.result = 0;
             }},
            {Funct::SetDMI,
             [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("SetDMI");
#endif
                 auto start_addr = std::get<size_t>(task.arg);
                 auto dmi_region = pycore.mem_regions[start_addr];
                 int res = pycore.lib.pydrofoil_cpu_set_dma_region(pycore.cpu, start_addr, dmi_region.size, dmi_region.ptr);
                 task.result = res;
             }},
            {Funct::GetExit,
             [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("GetExit");
#endif
                 task.result = pycore.lib.pydrofoil_get_htif_done(pycore.cpu);
             }},
            {Funct::SetHtifTohost,
             [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("SetHtifTohost");
#endif
                 auto tohost = std::get<size_t>(task.arg);
                 task.result = pycore.lib.pydrofoil_cpu_set_htif_tohost(pycore.cpu, tohost);
             }},
            {Funct::SetExtClint,
             [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("SetExtClint");
#endif
                 auto enable = std::get<size_t>(task.arg);
                 task.result = pycore.lib.pydrofoil_cpu_set_external_clint(pycore.cpu, enable);
             }},
            {Funct::SetTickFreq,
             [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("SetTickFreq");
#endif
                 auto insns_per_tick = std::get<size_t>(task.arg);
                 task.result = pycore.lib.pydrofoil_set_instructions_per_tick(pycore.cpu, insns_per_tick);
             }},
            {Funct::SetIrqLines, [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("SetIrqLines");
#endif
                 auto lines_ptr = std::get<uint64_t*>(task.arg);
                 pycore.lib.pydrofoil_set_interrupt_lines(pycore.cpu, lines_ptr);
                 task.result = 0;
             }},
            {Funct::SetHartId, [&pycore](PythonTask& task) {
#if PROFILING
                 Profiler t("SetHartId");
#endif
                 auto hartid = std::get<uint64_t>(task.arg);
                 task.result = pycore.lib.pydrofoil_cpu_set_hartid(pycore.cpu, hartid);
             }}};
}

} // namespace backend
