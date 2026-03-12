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
            Funct::SetBrkp, [&core](PythonTask &task){
                #if PROFILING
                    Profiler t("SetBrkp");
                #endif
                auto addr = std::get<size_t>(task.arg);
                int res = pydrofoil_cpu_set_breakpoint(core.cpu, addr);
                task.result.set_value(int(res == 0)); 
            }},
            {
            Funct::RemoveBrkp, [&core](PythonTask &task){
                #if PROFILING
                    Profiler t("RemoveBrkp");
                #endif
                auto addr = std::get<size_t>(task.arg);
                int res = pydrofoil_cpu_remove_breakpoint(core.cpu, addr);
                task.result.set_value(int(res == 0)); 
            }},
            {
            Funct::Simulate, [&core](PythonTask &task){
                #if PROFILING
                    Profiler t("Simulate");
                #endif
                auto cycles = std::get<size_t>(task.arg);
                auto n_steps = pydrofoil_cpu_simulate(core.cpu, cycles);
                //core.n_cycles = pydrofoil_cpu_cycles(core.cpu);
                task.result.set_value(n_steps); 
                core.memtask_cv.notify_one();
            }},
            {
            Funct::WriteReg, [&core](PythonTask &task){
                #if PROFILING
                    Profiler t("WriteReg");
                #endif
                auto args = std::get<WriteRegArgs>(task.arg);
                int res = pydrofoil_cpu_write_reg(core.cpu, args.reg_name, args.value);
                task.result.set_value(int(res == 0));
            }},
            {
            Funct::ReadReg, [&core](PythonTask &task){
                #if PROFILING
                    Profiler t("ReadReg");
                #endif
                auto reg_name = std::get<const char*>(task.arg);
                auto reg_value = pydrofoil_cpu_read_reg(core.cpu, reg_name);
                task.result.set_value(reg_value);
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