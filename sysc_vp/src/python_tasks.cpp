#include "python_tasks.h"
#include "core.h"

auto create_handlers(PydrofoilCore& core) // core == alias of the PydrofoilCore, we can use it inside the function as it is
    -> std::unordered_map<Funct, std::function<void(PythonTask&)>>
{
    return {
            {
            Funct::Init, [&core](PythonTask &task){  // the lambda keeps a referece of PydrofoilCore
                #if PROFILING
                    Profiler t("Init");
                #endif
                auto core_type = std::get<const char*>(task.arg);   
                core.cpu = pydrofoil_allocate_cpu(core_type, nullptr); 
                task.result.set_value(0);
            }},
            {
            Funct::SetCb, [&core](PythonTask &task){
                #if PROFILING
                    Profiler t("SetCb");
                #endif
                int res = pydrofoil_cpu_set_ram_read_write_callback(core.cpu, read_mem, write_mem, &core);//
                task.result.set_value(res);
            }},
            {
            Funct::GetCycles, [&core](PythonTask &task){
                #if PROFILING
                    Profiler t("GetCycles");
                #endif
                core.n_cycles = pydrofoil_cpu_cycles(core.cpu);
                task.result.set_value(core.n_cycles);
            }},
            {
            Funct::Simulate, [&core](PythonTask &task){
                #if PROFILING
                    Profiler t("Simulate");
                #endif
                auto cycles = std::get<size_t>(task.arg);
                pydrofoil_cpu_simulate(core.cpu, cycles);
                //core.n_cycles = pydrofoil_cpu_cycles(core.cpu);
                task.result.set_value(0); 
                core.memtask_cv.notify_one();
            }},
            {
            Funct::SetPc, [&core](PythonTask &task){
                #if PROFILING
                    Profiler t("SetPc");
                #endif
                auto pc_value = std::get<size_t>(task.arg);
                int res = pydrofoil_cpu_set_pc(core.cpu, pc_value);
                task.result.set_value(int(res == 0));
            }},
            {
            Funct::ReadPc, [&core](PythonTask &task){
                #if PROFILING
                    Profiler t("ReadPc");
                #endif
                auto pc_value = pydrofoil_cpu_pc(core.cpu);
                task.result.set_value(pc_value);
            }},
            {
            Funct::FreeCpu, [&core](PythonTask &task){
                #if PROFILING
                    Profiler t("FreeCpu");
                #endif
                pydrofoil_free_cpu(core.cpu);
                task.result.set_value(0);
            }},
            {
            Funct::SetVerbosity, [&core](PythonTask &task){
                #if PROFILING
                    Profiler t("SetVerbosity");
                #endif
                auto verbosity = std::get<size_t>(task.arg);
                pydrofoil_cpu_set_verbosity(core.cpu, verbosity);
                task.result.set_value(0);
            }},
            {
            Funct::SetDMI, [&core](PythonTask &task){
                #if PROFILING
                    Profiler t("SetDMI");
                #endif
                auto start_addr = std::get<size_t>(task.arg);
                auto dmi_region = core.mem_regions[start_addr];
                int res = pydrofoil_cpu_set_dma_region(core.cpu, start_addr, dmi_region.size, dmi_region.ptr);
                task.result.set_value(res);
            }},
            {
            Funct::SetMIP, [&core](PythonTask &task){
                #if PROFILING
                    Profiler t("RaiseIrq");
                #endif
                auto value = std::get<size_t>(task.arg);
                pydrofoil_set_interrupt_pending(core.cpu, value);
                task.result.set_value(0);
            }
            }
    };
}