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

// Keine PydrofoilCore& Parameter mehr!
namespace backend {
auto create_handlers() -> std::unordered_map<Funct, std::function<void(PythonTask&)>>
{
    return {
        {
            Funct::Init, [](PythonTask &task){  
                core::PydrofoilCore* core = task.caller_core; // <-- Core aus Task holen
                #if PROFILING
                    Profiler t("Init");
                #endif
                auto core_type = std::get<std::string>(task.arg); 
                core->cpu = pydrofoil_allocate_cpu(core_type.data(), nullptr); 
                task.result.set_value(0);
            }
        },
        {
            Funct::SetCb, [](PythonTask &task){
                core::PydrofoilCore* core = task.caller_core;
                #if PROFILING
                    Profiler t("SetCb");
                #endif
                // Wichtig: Beim Callback &core (Referenz) durch core (Pointer) ersetzen, 
                // da core jetzt schon ein Pointer ist!
                int res = pydrofoil_cpu_set_ram_read_write_callback(core->cpu, read_mem, write_mem, core);
                task.result.set_value(res);
            }
        },
        {
            Funct::GetCycles, [](PythonTask &task){
                core::PydrofoilCore* core = task.caller_core;
                #if PROFILING
                    Profiler t("GetCycles");
                #endif
                core->n_cycles = pydrofoil_cpu_cycles(core->cpu);
                task.result.set_value(core->n_cycles);
            }
        },
        {
            Funct::SetBrkp, [](PythonTask &task){
                core::PydrofoilCore* core = task.caller_core;
                #if PROFILING
                    Profiler t("SetBrkp");
                #endif
                auto addr = std::get<size_t>(task.arg);
                int res = pydrofoil_cpu_set_breakpoint(core->cpu, addr);
                task.result.set_value(int(res == 0)); 
            }
        },
        {
            Funct::RemoveBrkp, [](PythonTask &task){
                core::PydrofoilCore* core = task.caller_core;
                #if PROFILING
                    Profiler t("RemoveBrkp");
                #endif
                auto addr = std::get<size_t>(task.arg);
                int res = pydrofoil_cpu_remove_breakpoint(core->cpu, addr);
                task.result.set_value(int(res == 0)); 
            }
        },
        {
            Funct::Simulate, [](PythonTask &task){
                core::PydrofoilCore* core = task.caller_core;
                #if PROFILING
                    Profiler t("Simulate");
                #endif
                size_t n_steps = 0; 
                {
                    auto cycles = std::get<size_t>(task.arg);
                    n_steps = pydrofoil_cpu_simulate(core->cpu, cycles);
                }
                task.result.set_value(n_steps); 
                core->memtask_cv.notify_one();
            }
        },
        {
            Funct::WriteReg, [](PythonTask &task){
                core::PydrofoilCore* core = task.caller_core;
                #if PROFILING
                    Profiler t("WriteReg");
                #endif
                auto args = std::get<WriteRegArgs>(task.arg);
                int res = pydrofoil_cpu_write_reg(core->cpu, args.reg_name, args.value);
                task.result.set_value(int(res == 0));
            }
        },
        {
            Funct::ReadReg, [](PythonTask &task){
                core::PydrofoilCore* core = task.caller_core;
                #if PROFILING
                    Profiler t("ReadReg");
                #endif
                auto reg_name = std::get<std::string>(task.arg);
                auto reg_value = pydrofoil_cpu_read_reg(core->cpu, reg_name.data());
                task.result.set_value(reg_value);
            }
        },
        {
            Funct::FreeCpu, [](PythonTask &task){
                core::PydrofoilCore* core = task.caller_core;
                #if PROFILING
                    Profiler t("FreeCpu");
                #endif
                pydrofoil_free_cpu(core->cpu);
                task.result.set_value(0);
            }
        },
        {
            Funct::SetVerbosity, [](PythonTask &task){
                core::PydrofoilCore* core = task.caller_core;
                #if PROFILING
                    Profiler t("SetVerbosity");
                #endif
                auto verbosity = std::get<uint32_t>(task.arg);
                pydrofoil_cpu_set_verbosity(core->cpu, verbosity);
                task.result.set_value(0);
            }
        },
        {
            Funct::SetHartId, [](PythonTask &task){
                core::PydrofoilCore* core = task.caller_core;
                uint64_t id = std::get<uint64_t>(task.arg);
                pydrofoil_set_hartid(core->cpu, id);
                task.result.set_value(0);
            }
        },
        {
            Funct::SetDMI, [](PythonTask &task){
                core::PydrofoilCore* core = task.caller_core;
                #if PROFILING
                    Profiler t("SetDMI");
                #endif
                auto start_addr = std::get<size_t>(task.arg);
                auto dmi_region = core->mem_regions[start_addr];
                int res = pydrofoil_cpu_set_dma_region(core->cpu, start_addr, dmi_region.size, dmi_region.ptr);
                task.result.set_value(res);
            }
        },
        {
            Funct::SetMIP, [](PythonTask &task){
                core::PydrofoilCore* core = task.caller_core;
                #if PROFILING
                    Profiler t("RaiseIrq");
                #endif
                auto value = std::get<size_t>(task.arg);
                pydrofoil_set_interrupt_pending(core->cpu, value);
                task.result.set_value(0);
            }
        }
    };
}

} // namespace backend
