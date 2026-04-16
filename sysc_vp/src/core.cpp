#include "core.h"
#include <cstdio>
#include "riscv_arch.h"

PydrofoilCore::PydrofoilCore(const sc_core::sc_module_name& name, const char* core_type):
    vcml::processor(name,"riscv"),
    elf("elf",""),
    arch_name("arch_name","rv64"),
    verbosity("verbose",false),
    core_arch()
{
    if(arch_name.get() == "rv64"){
        core_arch = Model("rv64", 64, regdb_riscv, 33);
        set_little_endian(); // Otherwise the gdbserver inverts the bytes it reads
    }
    
    python_worker_thread = std::thread(&PydrofoilCore::python_worker_loop, this);

    PythonTask task;
    task.py_funct = Funct::Init;
    task.arg = core_type;
    std::future<uint64_t> done = task.result.get_future();

    {
        std::lock_guard lock(task_mutex);
        task_queue.push(std::move(task));
    }
    task_cv.notify_one(); // notify the waiting thread
    done.get(); // Wait for the result

    set_verbosity(verbosity.get());

    for (size_t i = 0; i < core_arch.reg_number(); ++i) 
        define_cpureg_rw(i, core_arch.get_regs_ptr()[i].gdb_name, core_arch.word_size());
}


void PydrofoilCore::test_reg_access(size_t regno){
    size_t write_val = 0;
    size_t read_old_val = 0;

    // Read old reg value
    read_reg_dbg(regno, &read_old_val, 8);
    mwr::log_info("Value from register x%ld: 0x%lx", regno, read_old_val);
    // Change reg value
    write_val = 0x10;
    write_reg_dbg(regno, (const void*) &write_val, 8);
    // Check if we changed it
    size_t read_new_val = 0;
    read_reg_dbg(regno, &read_new_val, 8);
    mwr::log_info("New value from register x%ld: 0x%lx", regno, read_new_val);
    // Restore old value
    write_reg_dbg(1, (const void*) &read_old_val, 8);
}


PydrofoilCore::~PydrofoilCore()
{
    if(cpu){
        PythonTask task;
        task.py_funct = Funct::FreeCpu;
        std::future<uint64_t> done = task.result.get_future();

        {
            std::lock_guard lock(task_mutex);
            task_queue.push(std::move(task));
            stop_worker = true;
        }
        task_cv.notify_one();
        done.get();
    }

    python_worker_thread.join();
}



void PydrofoilCore::notify_pending_irq(bool set){
    uint32_t mip_val;
    if(irq_num == MEIP)
        mip_val = set ? (MEIP_BIT) : 0;
    else if(irq_num == SEIP)
        mip_val = set ? (SEIP_BIT) : 0;

    PythonTask task;
    task.py_funct = Funct::SetMIP;
    task.arg = mip_val;
    std::future<uint64_t> done = task.result.get_future();

    {
        std::lock_guard lock(task_mutex);
        task_queue.push(std::move(task));
    }
    task_cv.notify_one(); // notify the waiting thread
    done.get(); // Wait for the result
}


void PydrofoilCore::interrupt(size_t irq, bool set) 
{
    if(set)
        is_irq_pending = true;
    else
        is_irq_pending = false;

    irq_num = irq;
}


bool PydrofoilCore::write_reg_dbg(size_t regno, const void* buf, size_t len)
{
    if(regno == 0)
        return true;

    if(len != core_arch.word_size()) 
        return false;

    PythonTask task;
    task.py_funct = Funct::WriteReg;

    std::string reg_name = core_arch.get_regs_ptr()[regno].x_name;
    size_t reg_val = *reinterpret_cast<const vcml::u64*>(buf);
    task.arg = WriteRegArgs{reg_name.c_str(), reg_val};

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
    if(regno == 0){
        *reinterpret_cast<vcml::u64*>(buf) = 0;
        return true;
    }

    if(len != core_arch.word_size()) 
        return false;

    std::string reg_name = core_arch.get_regs_ptr()[regno].x_name;

    PythonTask task;
    task.py_funct = Funct::ReadReg;
    task.arg = reg_name.c_str();
    std::future<uint64_t> done = task.result.get_future();

    {
        std::lock_guard lock(task_mutex);
        task_queue.push(std::move(task));
    }
    task_cv.notify_one(); // notify the waiting thread

    *reinterpret_cast<vcml::u64*>(buf) = done.get(); // Wait for the result
    return true;
}


void PydrofoilCore::check_for_dmi_regions()
{
    for(const tlm::tlm_dmi& dmi : data.dmi_cache().get_entries()) 
    {
        if(mem_regions.find(dmi.get_start_address()) == mem_regions.end()){
            uint64_t s = dmi.get_start_address();
            uint64_t e = dmi.get_end_address();
            auto size = e - s + 1; // +1 to include the last byte

            mem_regions.emplace(s, MemRegion{dmi.get_dmi_ptr(),s,size});

            PythonTask task;
            task.py_funct = Funct::SetDMI;
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


// Called from a coroutine
void PydrofoilCore::simulate(size_t cycles)
{
    if(is_irq_pending.has_value()){
        notify_pending_irq(is_irq_pending.value());
        is_irq_pending.reset();
    }

    PythonTask task;
    task.py_funct = Funct::Simulate;
    task.arg = step ? 1 : cycles;
    std::future<uint64_t> done = task.result.get_future();

    {
        std::lock_guard lock(task_mutex);
        task_queue.push(std::move(task));
    }

    task_cv.notify_one(); // notify the waiting thread
    

    while(done.wait_for(std::chrono::seconds(0)) != std::future_status::ready){
        
        MemAccess memtask;

        {
            std::unique_lock<std::mutex> lock(memtask_mutex);
            memtask_cv.wait(lock, [&]{return !memtask_queue.empty() ||
                                                (done.wait_for(std::chrono::seconds(0)) == std::future_status::ready);});
            
            if(!memtask_queue.empty()){
                memtask = std::move(memtask_queue.front());
                memtask_queue.pop();
            }
            else
                continue;
            
        }

        bool success = false;
        if(memtask.type == MemTask::Read){
            success = (data.read(memtask.addr, memtask.dest, memtask.size, vcml::SBI_NONE) == tlm::TLM_OK_RESPONSE);
            //memset(memtask.dest,0x297,8); // To be removed once the 0x1000 initial accesses are fixed
        }
        else
            success = (data.write(memtask.addr, &memtask.value, memtask.size, vcml::SBI_NONE) == tlm::TLM_OK_RESPONSE);
        
        if(!success)
            mwr::log_info("Memory access failed with address: %lx", memtask.addr);

        memtask.result.set_value(success);
    }

    size_t current_steps = done.get();
    if(!step && current_steps < cycles)
        handle_breakpoint_hit();
        
    n_cycles += current_steps;
    check_for_dmi_regions();
    step = false;
}


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
    PythonTask task;
    task.py_funct = Funct::SetBrkp;
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
    PythonTask task;
    task.py_funct = Funct::RemoveBrkp;
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
    //pydrofoil_cpu_reset(cpu);
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
    PythonTask task;
    task.py_funct = Funct::SetVerbosity;
    task.arg = (bool)value;
    std::future<uint64_t> done = task.result.get_future();

    {
        std::lock_guard lock(task_mutex);
        task_queue.push(std::move(task));
    }
    task_cv.notify_one(); // notify the waiting thread

    done.get(); // Wait for the result
}


void PydrofoilCore::python_worker_loop(){
    std::unordered_map<Funct, std::function<void(PythonTask&)>> handlers = create_handlers(*this);

    while(true) {
        PythonTask task;

        {   // We need unique_lock because:
            // 1. we need wait()
            // 2. wait can temporarely release the lock and reacquire once notified
            // Neither 1. nor 2. are supported by lock_guard
            std::unique_lock<std::mutex> lock(task_mutex); 
            task_cv.wait(lock, [this]{ return !task_queue.empty() || stop_worker;});

            if (stop_worker && task_queue.empty())
                break;
            
            task = std::move(task_queue.front()); // PythonTask has std::promise, not copyable!
            task_queue.pop();                      // pop: reason not to use eg vectors
        }   // --> lock released (out of scope)

        auto it = handlers.find(task.py_funct);
        if(it != handlers.end())
            it->second(task);
    }
}


void PydrofoilCore::end_of_elaboration()
{
    processor::end_of_elaboration();

    PythonTask task;
    task.py_funct = Funct::SetCb;
    std::future<uint64_t> done = task.result.get_future();

    {
        std::lock_guard lock(task_mutex);
        task_queue.push(std::move(task));
    }
    task_cv.notify_one(); // notify the waiting thread
    done.get(); // Wait for the result
}