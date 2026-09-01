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
 
 #define PYCORE_LOG(core_ref, stream_args)                       \
     do {                                                        \
         std::lock_guard<std::mutex> lock(core_ref.m_log_mutex); \
         if(core_ref.m_log_file.is_open()) {                     \
             core_ref.m_log_file << stream_args << std::endl;    \
             core_ref.m_log_file.flush();                        \
         } else {                                                \
             std::cout << stream_args << std::endl;              \
         }                                                       \
     } while(0)
 // ---------------------------------------
 
 namespace backend {
 
 auto create_handlers(core::PydrofoilCore& pycore) -> std::unordered_map<Funct, std::function<void(PythonTask&)>>
 {
     return {
         {Funct::Init,
          [&pycore](PythonTask& task) { // the lambda should keep a referece of PydrofoilCore
 #if PROFILING
              Profiler t("Init");
 #endif
              auto core_type = std::get<std::string>(task.arg);
              pycore.cpu = pycore.m_pydrofoil_allocate_cpu(core_type.data(), nullptr);
              task.result.set_value(0);
          }},
         {Funct::SetCb,
          [&pycore](PythonTask& task) {
 #if PROFILING
              Profiler t("SetCb");
 #endif
              int res = pycore.m_pydrofoil_cpu_set_ram_read_write_callback(pycore.cpu, read_mem, write_mem, &pycore); //
              task.result.set_value(res);
          }},
         {Funct::GetCycles,
          [&pycore](PythonTask& task) {
 #if PROFILING
              Profiler t("GetCycles");
 #endif
              pycore.n_cycles = pycore.m_pydrofoil_cpu_cycles(pycore.cpu);
              task.result.set_value(pycore.n_cycles);
          }},
         {Funct::SetBrkp,
          [&pycore](PythonTask& task) {
 #if PROFILING
              Profiler t("SetBrkp");
 #endif
              auto addr = std::get<size_t>(task.arg);
              int res = pycore.m_pydrofoil_cpu_set_breakpoint(pycore.cpu, addr);
              task.result.set_value(int(res == 0));
          }},
         {Funct::RemoveBrkp,
          [&pycore](PythonTask& task) {
 #if PROFILING
              Profiler t("RemoveBrkp");
 #endif
              auto addr = std::get<size_t>(task.arg);
              int res = pycore.m_pydrofoil_cpu_remove_breakpoint(pycore.cpu, addr);
              task.result.set_value(int(res == 0));
          }},
         {Funct::Simulate,
          [&pycore, hartid_checked = false](PythonTask& task) mutable {
 #if PROFILING
              Profiler t("Simulate");
 #endif
              // What the guest actually sees once it starts fetching: a later
              // cpu.reset() on the python side would silently wipe mhartid.
              if(!hartid_checked) {
                  hartid_checked = true;
                  uint64_t seen = pycore.m_pydrofoil_cpu_read_reg(pycore.cpu, "mhartid");
                  PYCORE_LOG(pycore, "Hart " << pycore.m_hart_id << " | mhartid at first simulate: " << seen);
                  if(seen != pycore.m_hart_id) {
                      mwr::log_warn("Hart %lu starts executing with mhartid %lu", pycore.m_hart_id,
                                    (unsigned long) seen);
                  }
              }

              auto cycles = std::get<size_t>(task.arg);
              //  PYCORE_LOG(pycore, "Hart " << pycore.m_hart_id << " | PYTHON WORKER STARTED" << std::endl);
              auto n_steps = pycore.m_pydrofoil_cpu_simulate(pycore.cpu, cycles);
              // pycore.n_cycles = core.m_pydrofoil_cpu_cycles(pycore.cpu);
              task.result.set_value(n_steps);
              //  PYCORE_LOG(pycore, "Hart " << pycore.m_hart_id << " | PYTHON WORKER FNISHED" << std::endl);
              {
                  std::lock_guard<std::mutex> lock(pycore.memtask_mutex);
                  pycore.sim_done_flag = true;
              }
              pycore.memtask_cv.notify_one();
              //  PYCORE_LOG(pycore, "Hart " << pycore.m_hart_id << " | PYTHON WORKER RETURNING" << std::endl);
          }},
         {Funct::WriteReg,
          [&pycore](PythonTask& task) {
 #if PROFILING
              Profiler t("WriteReg");
 #endif
              auto args = std::get<WriteRegArgs>(task.arg);
              int res = pycore.m_pydrofoil_cpu_write_reg(pycore.cpu, args.reg_name, args.value);
              task.result.set_value(int(res == 0));
          }},
         {Funct::ReadReg,
          [&pycore](PythonTask& task) {
 #if PROFILING
              Profiler t("ReadReg");
 #endif
              auto reg_name = std::get<std::string>(task.arg);
              auto reg_value = pycore.m_pydrofoil_cpu_read_reg(pycore.cpu, reg_name.c_str());
              task.result.set_value(reg_value);
          }},
         {Funct::FreeCpu,
          [&pycore](PythonTask& task) {
 #if PROFILING
              Profiler t("FreeCpu");
 #endif
              pycore.m_pydrofoil_free_cpu(pycore.cpu);
              task.result.set_value(0);
          }},
         {Funct::SetVerbosity,
          [&pycore](PythonTask& task) {
 #if PROFILING
              Profiler t("SetVerbosity");
 #endif
              auto verbosity = std::get<uint32_t>(task.arg);
              pycore.m_pydrofoil_cpu_set_verbosity(pycore.cpu, verbosity);
              task.result.set_value(0);
          }},
         {Funct::SetHartId,
          [&pycore](PythonTask& task) {
              // We assume arg holds the ID (std::variant check)
              uint64_t id = std::get<uint64_t>(task.arg);
              pycore.m_pydrofoil_set_hartid(pycore.cpu, id);

              uint64_t readback = pycore.m_pydrofoil_cpu_read_reg(pycore.cpu, "mhartid");
              PYCORE_LOG(pycore, "Hart " << pycore.m_hart_id << " | mhartid set to " << id << ", read back "
                                         << readback);
              if(readback != id) {
                  mwr::log_warn("Hart %lu: mhartid readback %lu does not match requested %lu", pycore.m_hart_id,
                                (unsigned long) readback, (unsigned long) id);
              }
              task.result.set_value(0);
          }},
         {Funct::SetDMI,
          [&pycore](PythonTask& task) {
 #if PROFILING
              Profiler t("SetDMI");
 #endif
              auto start_addr = std::get<size_t>(task.arg);
              auto dmi_region = pycore.mem_regions[start_addr];
              int res = pycore.m_pydrofoil_cpu_set_dma_region(pycore.cpu, start_addr, dmi_region.size, dmi_region.ptr);
              task.result.set_value(res);
          }},
         {Funct::SetMIP, [&pycore](PythonTask& task) {
 #if PROFILING
              Profiler t("RaiseIrq");
 #endif
              auto value = std::get<size_t>(task.arg);
              pycore.m_pydrofoil_set_interrupt_pending(pycore.cpu, value);
              task.result.set_value(0);
          }}};
 }
 
 } // namespace backend