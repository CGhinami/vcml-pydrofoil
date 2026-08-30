/******************************************************************************
 *                                                                            *
 * Copyright 2026 Chiara Ghinami                                              *
 *                                                                            *
 * This software is licensed under the MIT license found in the               *
 * LICENSE file at the root directory of this source tree.                    *
 *                                                                            *
 ******************************************************************************/

#include "core.h"
#include <cstdio>
#include "riscv_arch.h"
#include <dlfcn.h>
#include <string>
#include <unistd.h> // <-- Add this for getpid()
#include <filesystem>

namespace core {
PydrofoilCore::PydrofoilCore(const sc_core::sc_module_name& name, uint64_t hart_id):
    vcml::processor(name, "riscv"),
    elf("elf", ""),
    arch_name("arch_name", "rv64"),
    verbosity("verbose", false),
    cpu(nullptr),
    use_dmi(true),
    n_cycles(0),
    step(true), // For the first execution we want just 1 instruction to run
    stop_worker(false),
    core_arch(arch_name.c_str(), arch_name == "rv64" ? 64 : 32, architecture::regdb_riscv, 33),
    m_hart_id(hart_id)
{
    std::filesystem::create_directories("logs"); // Sicherstellen, dass der Ordner existiert
    std::string log_path = "logs/core" + std::to_string(hart_id) + "_debug.txt";
    m_log_file.open(log_path, std::ios::out | std::ios::trunc);
    if(m_log_file.is_open()) {
        mwr::log_info("Opened debug log file for Hart %lu at %s", hart_id, log_path.c_str());
    }

    // --- ISOLATED LIBRARY SETUP ---
    std::string base_lib = "./libpydrofoilcapi_cffi.so";
    std::string isolated_dir = "/tmp/isolated_libs";
    std::filesystem::create_directories(isolated_dir);

    std::string inst_lib = isolated_dir + "/libpydrofoil_hart" + std::to_string(hart_id) + "_pid" +
                           std::to_string(getpid()) + ".so";

    try {
        std::filesystem::copy_file(base_lib, inst_lib, std::filesystem::copy_options::overwrite_existing);
        mwr::log_info("Created isolated library instance for hart %lu at %s", hart_id, inst_lib.c_str());
    } catch(std::filesystem::filesystem_error& e) {
        VCML_ERROR("Failed to copy library for multicore isolation: %s", e.what());
    }

    m_pydrofoil_handle = dlmopen(LM_ID_NEWLM, inst_lib.c_str(), RTLD_NOW | RTLD_LOCAL);
    // m_pydrofoil_handle = dlopen(inst_lib.c_str(), RTLD_NOW | RTLD_LOCAL); // use this for async=false
    VCML_ERROR_ON(!m_pydrofoil_handle, "Could not open unique Pydrofoil library '%s': %s", inst_lib.c_str(), dlerror());

    // --- 2. MAP THE FUNCTION POINTERS ---
    m_pydrofoil_set_hartid = (int (*)(void*, uint64_t)) dlsym(m_pydrofoil_handle, "pydrofoil_set_hartid");
    m_pydrofoil_allocate_cpu = (void* (*) (const char*, const char*) ) dlsym(m_pydrofoil_handle,
                                                                             "pydrofoil_allocate_cpu");
    m_pydrofoil_cpu_set_ram_read_write_callback = (int (*)(
        void*, int (*)(void*, uint64_t, int, void*, void*), int (*)(void*, uint64_t, int, uint64_t, void*),
        void*)) dlsym(m_pydrofoil_handle, "pydrofoil_cpu_set_ram_read_write_callback");
    m_pydrofoil_cpu_cycles = (uint64_t (*)(void*)) dlsym(m_pydrofoil_handle, "pydrofoil_cpu_cycles");
    m_pydrofoil_cpu_set_breakpoint = (int (*)(void*, uint64_t)) dlsym(m_pydrofoil_handle,
                                                                      "pydrofoil_cpu_set_breakpoint");
    m_pydrofoil_cpu_remove_breakpoint = (int (*)(void*, uint64_t)) dlsym(m_pydrofoil_handle,
                                                                         "pydrofoil_cpu_remove_breakpoint");
    m_pydrofoil_cpu_simulate = (int (*)(void*, size_t)) dlsym(m_pydrofoil_handle, "pydrofoil_cpu_simulate");
    m_pydrofoil_cpu_write_reg = (int (*)(void*, char const*, uint64_t)) dlsym(m_pydrofoil_handle,
                                                                              "pydrofoil_cpu_write_reg");
    m_pydrofoil_cpu_read_reg = (uint64_t (*)(void*, char const*)) dlsym(m_pydrofoil_handle, "pydrofoil_cpu_read_reg");
    m_pydrofoil_free_cpu = (int (*)(void*)) dlsym(m_pydrofoil_handle, "pydrofoil_free_cpu");
    m_pydrofoil_cpu_set_verbosity = (int (*)(void*, int)) dlsym(m_pydrofoil_handle, "pydrofoil_cpu_set_verbosity");
    m_pydrofoil_cpu_set_dma_region = (int (*)(void*, uint64_t, uint64_t, uint8_t*)) dlsym(
        m_pydrofoil_handle, "pydrofoil_cpu_set_dma_region");
    m_pydrofoil_set_interrupt_pending = (int (*)(void*, uint32_t)) dlsym(m_pydrofoil_handle,
                                                                         "pydrofoil_set_interrupt_pending");

    VCML_ERROR_ON(!m_pydrofoil_allocate_cpu, "Could not load symbol: %s", dlerror());

    mwr::log_info("Running with arch: %d bit", 8 * core_arch.word_size());
    set_little_endian(); // Otherwise the gdbserver inverts the bytes it reads

    python_worker_thread = std::thread(&PydrofoilCore::python_worker_loop, this);

    backend::PythonTask task;
    task.py_funct = backend::Funct::Init;
    task.arg = arch_name;
    std::future<uint64_t> done = task.result.get_future();

    {
        std::lock_guard lock(task_mutex);
        task_queue.push(std::move(task));
    }
    task_cv.notify_one(); // notify the waiting thread
    done.get();           // Wait for the result

    set_verbosity(verbosity.get());

    for(size_t i = 0; i < core_arch.reg_number(); ++i)
        define_cpureg_rw(i, core_arch.get_regs_ptr()[i].gdb_name, core_arch.word_size());
    // std::cout << "DEBUG: C++ Constructor for " << name << " has hart_id: " << m_hart_id << " hart_id value is: " <<
    // hart_id << std::endl;
}

void PydrofoilCore::test_reg_access(size_t regno)
{
    size_t write_val = 0;
    size_t read_old_val = 0;

    // Read old reg value
    read_reg_dbg(regno, &read_old_val, core_arch.word_size());
    mwr::log_info("Value from register x%ld: 0x%lx", regno, read_old_val);
    // Change reg value
    write_val = 0x10;
    write_reg_dbg(regno, (const void*) &write_val, core_arch.word_size());
    // Check if we changed it
    size_t read_new_val = 0;
    read_reg_dbg(regno, &read_new_val, core_arch.word_size());
    mwr::log_info("New value from register x%ld: 0x%lx", regno, read_new_val);
    // Restore old value
    write_reg_dbg(1, (const void*) &read_old_val, core_arch.word_size());
}

PydrofoilCore::~PydrofoilCore()
{
    if(cpu) {
        backend::PythonTask task;
        task.py_funct = backend::Funct::FreeCpu;
        std::future<uint64_t> done = task.result.get_future();

        {
            std::lock_guard lock(task_mutex);
            task_queue.push(std::move(task));
            stop_worker = true;
        }
        task_cv.notify_one();
        done.get();
    }
    if(m_log_file.is_open()) {
        m_log_file.close();
    }

    python_worker_thread.join();
    // --- ADDED FOR DLOPEN ---
    if(m_pydrofoil_handle) {
        dlclose(m_pydrofoil_handle);
    }
    // ------------------------
}

void PydrofoilCore::notify_pending_irq(bool set)
{
    size_t mip_val;
    if(irq_num == MEIP)
        mip_val = set ? (MEIP_BIT) : 0;
    else if(irq_num == SEIP)
        mip_val = set ? (SEIP_BIT) : 0;
    else if(irq_num == MSIP)
        mip_val = set ? (MSIP_BIT) : 0;
    else if(irq_num == MTIP)
        mip_val = set ? (MTIP_BIT) : 0;

    backend::PythonTask task;
    task.py_funct = backend::Funct::SetMIP;
    task.arg = mip_val;
    std::future<uint64_t> done = task.result.get_future();
    CORE_LOG("interrupt task for core " << m_hart_id << " with irq_num: " << irq_num << " and mip_val: " << mip_val
                                        << " pushed to task queue");

    {
        std::lock_guard lock(task_mutex);
        task_queue.push(std::move(task));
    }
    task_cv.notify_one(); // notify the waiting thread
    done.get();           // Wait for the result
    CORE_LOG("interrupt task for core " << m_hart_id << " with irq_num: " << irq_num << " and mip_val: " << mip_val
                                        << " completed");
    if(irq_num == 2 && mip_val == 0 && m_hart_id == 0) {
        CORE_LOG("Deadlock detected");
    }
}

void PydrofoilCore::interrupt(size_t irq, bool set)
{
    is_irq_pending = set;
    irq_num = irq;
}

bool PydrofoilCore::write_reg_dbg(size_t regno, const void* buf, size_t len)
{
    if(regno == 0)
        return true;

    if(len != core_arch.word_size())
        return false;

    backend::PythonTask task;
    task.py_funct = backend::Funct::WriteReg;
    size_t reg_val;

    std::string reg_name = core_arch.get_regs_ptr()[regno].x_name;
    std::memcpy(&reg_val, buf, len);
    task.arg = backend::WriteRegArgs{reg_name.c_str(), reg_val};

    std::future<uint64_t> done = task.result.get_future();

    {
        std::lock_guard lock(task_mutex);
        task_queue.push(std::move(task));
    }
    task_cv.notify_one(); // notify the waiting thread

    return done.get(); // Wait for the result
}

bool PydrofoilCore::read_reg_dbg(size_t regno, void* buf, size_t len)
{
    if(regno == 0) {
        std::memcpy(buf, &regno, core_arch.word_size()); // We just copy 0
        return true;
    }

    if(len != core_arch.word_size())
        return false;

    std::string reg_name = core_arch.get_regs_ptr()[regno].x_name;

    backend::PythonTask task;
    task.py_funct = backend::Funct::ReadReg;
    task.arg = reg_name;
    std::future<uint64_t> done = task.result.get_future();

    {
        std::lock_guard lock(task_mutex);
        task_queue.push(std::move(task));
    }
    task_cv.notify_one(); // notify the waiting thread

    uint64_t reg_val = done.get();   // Wait for the result
    std::memcpy(buf, &reg_val, len); // Truncates if sizeof reg_val > word_size (only works with little-endian!)

    return true;
}

void PydrofoilCore::check_for_dmi_regions()
{
    for(const tlm::tlm_dmi& dmi : data.dmi_cache().get_entries()) {
        if(mem_regions.find(dmi.get_start_address()) == mem_regions.end()) {
            uint64_t s = dmi.get_start_address();
            uint64_t e = dmi.get_end_address();
            auto size = e - s + 1; // +1 to include the last byte

            mem_regions.emplace(s, MemRegion{dmi.get_dmi_ptr(), s, size});

            backend::PythonTask task;
            task.py_funct = backend::Funct::SetDMI;
            task.arg = s;
            std::future<uint64_t> done = task.result.get_future();

            {
                std::lock_guard lock(task_mutex);
                task_queue.push(std::move(task));
            }
            task_cv.notify_one(); // notify the waiting thread
            if(done.get() != 0)
                mwr::log_info("Setting DMI pointer failed");
        }
    }
}

void PydrofoilCore::sc_sync_catch_ex(std::function<void(void)> job)
{
    try {
        CORE_LOG("[DEBUG] Hart " << m_hart_id << " | Entering sc_sync_catch_ex");
        vcml::sc_sync(std::move(job));
        CORE_LOG("[DEBUG] Hart " << m_hart_id << " | sc_sync_catch_ex executed successfully");
    } catch(...) {
        // Catch all exceptions to prevent SystemC from terminating the simulation
        mwr::log_error("Exception caught in sc_sync_catch_ex");
    }
}

// Called from a coroutine
void PydrofoilCore::simulate(size_t cycles)
{
    if(is_irq_pending.has_value()) {
        notify_pending_irq(is_irq_pending.value());
        CORE_LOG("Hart " << m_hart_id << " Interrupt handling done");
        is_irq_pending.reset();
    }

    sim_done_flag = false;
    backend::PythonTask task;
    task.py_funct = backend::Funct::Simulate;
    task.arg = step ? 1 : cycles;
    std::future<uint64_t> done = task.result.get_future();

    {
        std::lock_guard lock(task_mutex);
        task_queue.push(std::move(task));
    }

    task_cv.notify_one(); // notify the waiting thread
    CORE_LOG("Hart " << m_hart_id << "start: PydrofoilCore::simulate");
    while(1) {
        MemAccess memtask;

        {
            std::unique_lock<std::mutex> lock(memtask_mutex);

            // wait_for blockiert maximal für 3 echte Host-Sekunden
            memtask_cv.wait(lock, [&] { return !memtask_queue.empty() || sim_done_flag; });
            if(!memtask_queue.empty()) {
                memtask = std::move(memtask_queue.front());
                memtask_queue.pop();
            } else if(sim_done_flag) {
                CORE_LOG("Hart " << m_hart_id << " | Breaking out of memtask loop because sim task is ready");
                break;
            } else {
                CORE_LOG("Hart " << m_hart_id << " | DANGER ELSE CONDITION");
                continue;
            }
        }

        bool success = false;
        if(memtask.type == MemTask::Read) {
            if(vcml::is_thread()) {
                success = (data.read(memtask.addr, memtask.dest, memtask.size, vcml::SBI_NONE) == tlm::TLM_OK_RESPONSE);
            } else {
                // mwr::log_info("os thread!");
                sc_sync_catch_ex([&]() {
                    success = (data.read(memtask.addr, memtask.dest, memtask.size, vcml::SBI_NONE) ==
                               tlm::TLM_OK_RESPONSE);
                });
            }
        } else {
            if(vcml::is_thread()) {
                success = (data.write(memtask.addr, &memtask.value, memtask.size, vcml::SBI_NONE) ==
                           tlm::TLM_OK_RESPONSE);
            } else {
                // mwr::log_info("os thread!");
                sc_sync_catch_ex([&]() {
                    success = (data.write(memtask.addr, &memtask.value, memtask.size, vcml::SBI_NONE) ==
                               tlm::TLM_OK_RESPONSE);
                });
            }
        }

        if(!success) {
            mwr::log_info("Memory access failed with address: %lx", memtask.addr);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        memtask.result.set_value(success);
    }

    size_t current_steps = done.get();
    bool brkpt_hit = current_steps > 0 && current_steps < cycles;

    if(!step && brkpt_hit)
        handle_breakpoint_hit();

    n_cycles += current_steps;
    check_for_dmi_regions();
    step = false;
    CORE_LOG("Hart " << m_hart_id << " | end: PydrofoilCore::simulate");
}

// void PydrofoilCore::simulate(size_t cycles)
// {
//     if(is_irq_pending.has_value()) {
//         notify_pending_irq(is_irq_pending.value());
//         is_irq_pending.reset();
//     }

//     backend::PythonTask task;
//     task.py_funct = backend::Funct::Simulate;
//     task.arg = step ? 1 : cycles;
//     std::future<uint64_t> done = task.result.get_future();

//     {
//         std::lock_guard lock(task_mutex);
//         task_queue.push(std::move(task));
//     }
//     std::cout << "DEBUG: C++ simulate() called on core with hart id " << m_hart_id << std::endl;
//     task_cv.notify_one(); // notify the waiting thread

//     while(done.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
//         MemAccess memtask;

//         {
//             std::unique_lock<std::mutex> lock(memtask_mutex);
//             memtask_cv.wait(lock, [&]
//                             { return !memtask_queue.empty() ||
//                                      (done.wait_for(std::chrono::seconds(0)) == std::future_status::ready); });

//             if (!memtask_queue.empty())
//             {
//                 memtask = std::move(memtask_queue.front());
//                 memtask_queue.pop();
//             }
//             else
//                 continue;
//         }

//         bool success = false;
//         if(memtask.type == MemTask::Read) {
//             success = (data.read(memtask.addr, memtask.dest, memtask.size, vcml::SBI_NONE) == tlm::TLM_OK_RESPONSE);
//             // memset(memtask.dest,0x297,8); // To be removed once the 0x1000 initial accesses are fixed
//             std::cout << "handling memtask for hart" << m_hart_id << std::endl;
//         } else {
//             success = (data.write(memtask.addr, &memtask.value, memtask.size, vcml::SBI_NONE) ==
//             tlm::TLM_OK_RESPONSE); std::cout << "handling memtask for hart" << m_hart_id << std::endl;
//         }
//         if (!success)
//             mwr::log_info("Memory access failed with address: %lx", memtask.addr);

//         memtask.result.set_value(success);
//     }

//     size_t current_steps = done.get();
//     bool brkpt_hit = current_steps > 0 && current_steps < cycles;

//     if(!step && brkpt_hit)
//         handle_breakpoint_hit();

//     n_cycles += current_steps;
//     check_for_dmi_regions();
//     step = false;
// }

void PydrofoilCore::handle_breakpoint_hit()
{
    mwr::log_info("Breakpoint hit");
    size_t pc_val = 0;
    int reg_idx = core_arch.find_reg_idx("pc");

    read_reg_dbg(reg_idx, &pc_val, core_arch.word_size());
    notify_breakpoint_hit(pc_val);
}

bool PydrofoilCore::insert_breakpoint(vcml::u64 addr)
{
    backend::PythonTask task;
    task.py_funct = backend::Funct::SetBrkp;
    task.arg = addr;
    std::future<uint64_t> done = task.result.get_future();

    {
        std::lock_guard lock(task_mutex);
        task_queue.push(std::move(task));
    }

    task_cv.notify_one(); // notify the waiting thread
    return done.get();
}

bool PydrofoilCore::remove_breakpoint(vcml::u64 addr)
{
    backend::PythonTask task;
    task.py_funct = backend::Funct::RemoveBrkp;
    task.arg = addr;
    std::future<uint64_t> done = task.result.get_future();

    {
        std::lock_guard lock(task_mutex);
        task_queue.push(std::move(task));
    }

    task_cv.notify_one(); // notify the waiting thread
    return done.get();
}

// Called from a coroutine
vcml::u64 PydrofoilCore::cycle_count() const
{
    return n_cycles;
}

void PydrofoilCore::reset()
{
    // 1. Run the standard VCML reset (clears PC, registers, etc.)
    vcml::processor::reset();

    // 2. Force the Hart ID again
    // This ensures that even if the Python object was recreated,
    // it gets the correct ID before execution starts.
    backend::PythonTask task;
    task.py_funct = backend::Funct::SetHartId;
    task.arg = m_hart_id; // Use the member variable we saved
    // std::cout << "DEBUG: Calling set_hartid in reset with hart_id:----------------------------- " << m_hart_id <<
    // std::endl;

    std::future<uint64_t> done = task.result.get_future();
    {
        std::lock_guard lock(task_mutex);
        task_queue.push(std::move(task));
    }
    task_cv.notify_one();
    done.get(); // Wait for confirmation
}

/* How it would look like without the std::future
   Pros: faster (see profiling)
   Cons: error prone
   --> Unless in the profiling we see that it's the bottleneck we stick with it
void PydrofoilCore::set_pc(vcml::u64 value)
{
    auto task = std::make_shared<PythonTask>(); //both threads refer to the same object
    task->py_funct = Funct::SetPc;
    task->arg = value;

    {
        std::lock_guard lock(task_mutex);
        task_queue.push(task);  // Now we're copying a pointer to the struct
    }
    task_cv.notify_one(); // notify the waiting thread

    {
        std::unique_lock lock(task->done_mutex);
        task->done_cv.wait(lock, [&] { return task->done; });
    }
    return done.value;
}
*/

void PydrofoilCore::set_verbosity(bool value)
{
    backend::PythonTask task;
    task.py_funct = backend::Funct::SetVerbosity;
    task.arg = (uint32_t) value;
    std::future<uint64_t> done = task.result.get_future();

    {
        std::lock_guard lock(task_mutex);
        task_queue.push(std::move(task));
    }
    task_cv.notify_one(); // notify the waiting thread

    done.get(); // Wait for the result
}

void PydrofoilCore::python_worker_loop()
{
    std::unordered_map<backend::Funct, std::function<void(backend::PythonTask&)>> handlers = backend::create_handlers(
        *this);

    while(true) {
        backend::PythonTask task;

        { // We need unique_lock because:
            // 1. we need wait()
            // 2. wait can temporarely release the lock and reacquire once notified
            // Neither 1. nor 2. are supported by lock_guard
            std::unique_lock<std::mutex> lock(task_mutex);
            task_cv.wait(lock, [this] { return !task_queue.empty() || stop_worker; });

            if(stop_worker && task_queue.empty())
                break;

            task = std::move(task_queue.front()); // PythonTask has std::promise, not copyable!
            task_queue.pop();                     // pop: reason not to use eg vectors
        } // --> lock released (out of scope)

        auto it = handlers.find(task.py_funct);
        if(it != handlers.end()) {
            // std::cout << "DEBUG: Handling task for hart " << m_hart_id << " with function " <<
            // static_cast<int>(task.py_funct) << std::endl;
            it->second(task);
        }
        CORE_LOG("Python worker: Task for hart " << m_hart_id << " with function " << static_cast<int>(task.py_funct)
                                                 << " completed");
    }
}

void PydrofoilCore::end_of_elaboration()
{
    processor::end_of_elaboration();

    backend::PythonTask task;
    task.py_funct = backend::Funct::SetCb;
    std::future<uint64_t> done = task.result.get_future();

    {
        std::lock_guard lock(task_mutex);
        task_queue.push(std::move(task));
    }
    task_cv.notify_one(); // notify the waiting thread
    done.get();           // Wait for the result
}

} // namespace core
