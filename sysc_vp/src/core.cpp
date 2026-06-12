#include "core.h"
#include <cstdio>
#include "riscv_arch.h"
#include <string>
// #include <filesystem>
// #include <unistd.h> // <-- Add this for getpid()
std::thread PydrofoilCore::global_python_worker_thread;
std::queue<PythonTask> PydrofoilCore::global_task_queue;
std::condition_variable PydrofoilCore::global_task_cv;
std::mutex PydrofoilCore::global_task_mutex;
bool PydrofoilCore::global_worker_running = false;

PydrofoilCore::PydrofoilCore(const sc_core::sc_module_name& name, uint64_t hart_id) : 
    vcml::processor(name,"riscv"),
    elf("elf",""),
    arch_name("arch_name","rv64"),
    verbosity("verbose",false),
    core_arch()
{
    char* core_type = (char*)"rv32";
    if(arch_name.get() == "rv64"){
        core_arch = Model("rv64", 64, regdb_riscv, 33);
        core_type = (char*)"rv64";
    }else
        core_arch = Model("rv32", 32, regdb_riscv, 33);
    
    mwr::log_info("Running with arch: %d bit", 8*core_arch.word_size());
    set_little_endian(); // Otherwise the gdbserver inverts the bytes it reads

    {
        std::lock_guard<std::mutex> lock(global_task_mutex);
        if (!global_worker_running) {
            global_python_worker_thread = std::thread(&PydrofoilCore::global_python_worker_loop);
            global_worker_running = true;
        }
    }
    
    PythonTask task;
    task.py_funct = Funct::Init;
    task.arg = core_type;
    task.caller_core = this;
    std::future<uint64_t> done = task.result.get_future();

    {
        std::lock_guard lock(global_task_mutex);
        global_task_queue.push(std::move(task));
    }
    global_task_cv.notify_one();; // notify the waiting thread
    done.get(); // Wait for the result

    set_verbosity(verbosity.get());
    m_hart_id = hart_id;  // Save it in the member variable for later use in reset()

    for (size_t i = 0; i < core_arch.reg_number(); ++i) 
        define_cpureg_rw(i, core_arch.get_regs_ptr()[i].gdb_name, core_arch.word_size());
}


void PydrofoilCore::test_reg_access(size_t regno){
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
    if(cpu){
        PythonTask task;
        task.py_funct = Funct::FreeCpu;
        task.caller_core = this; // <-- CRITICAL: Tell Python WHICH cpu to free!
        
        std::future<uint64_t> done = task.result.get_future();

        {
            std::lock_guard<std::mutex> lock(global_task_mutex); // Use global mutex
            global_task_queue.push(std::move(task));             // Use global queue
        }
        global_task_cv.notify_one();;
        done.get(); // Wait for Python to actually free the memory
    }

    // REMOVED: stop_worker = true;
    // REMOVED: python_worker_thread.join();
}

void PydrofoilCore::shutdown_worker()
{
    {
        std::lock_guard<std::mutex> lock(global_task_mutex);
        if (!global_worker_running) {
            return; // Already stopped, nothing to do
        }
        // Tell the loop to exit
        global_worker_running = false; 
    }
    
    // Wake up the worker thread so it evaluates the `!global_worker_running` condition
    global_task_cv.notify_all(); 

    // Wait for the thread to cleanly exit
    if (global_python_worker_thread.joinable()) {
        global_python_worker_thread.join();
    }
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
    task.caller_core = this;
    std::future<uint64_t> done = task.result.get_future();

    {
        std::lock_guard lock(global_task_mutex);
        global_task_queue.push(std::move(task));
    }
    global_task_cv.notify_one();; // notify the waiting thread
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
    task.caller_core = this;
    size_t reg_val;

    std::string reg_name = core_arch.get_regs_ptr()[regno].x_name;
    std::memcpy(&reg_val, buf, len);
    task.arg = WriteRegArgs{reg_name.c_str(), reg_val};

    std::future<uint64_t> done = task.result.get_future();

    {
        std::lock_guard lock(global_task_mutex);
        global_task_queue.push(std::move(task));
    }
    global_task_cv.notify_one();; // notify the waiting thread

    return done.get(); // Wait for the result

}


bool PydrofoilCore::read_reg_dbg(size_t regno, void* buf, size_t len)
{
    if(regno == 0){
        std::memcpy(buf, &regno, core_arch.word_size()); // We just copy 0
        return true;
    }

    if(len != core_arch.word_size()) 
        return false;

    std::string reg_name = core_arch.get_regs_ptr()[regno].x_name;

    PythonTask task;
    task.py_funct = Funct::ReadReg;
    task.arg = reg_name.c_str();
    task.caller_core = this;
    std::future<uint64_t> done = task.result.get_future();

    {
        std::lock_guard lock(global_task_mutex);
        global_task_queue.push(std::move(task));
    }
    global_task_cv.notify_one();; // notify the waiting thread

    uint64_t reg_val = done.get(); // Wait for the result
    std::memcpy(buf, &reg_val, len); // Truncates if sizeof reg_val > word_size (only works with little-endian!)

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
            task.caller_core = this;
            std::future<uint64_t> done = task.result.get_future();

            {
                std::lock_guard lock(global_task_mutex);
                global_task_queue.push(std::move(task));
            }
            global_task_cv.notify_one();; // notify the waiting thread
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
    // std::cout << "SIMULATE with hard_i: " << m_hart_id << std::endl;
    PythonTask task;
    task.py_funct = Funct::Simulate;
    task.arg = step ? 1 : cycles;
    task.caller_core = this;
    std::future<uint64_t> done = task.result.get_future();

    {
        std::lock_guard lock(global_task_mutex);
        global_task_queue.push(std::move(task));
    }

    global_task_cv.notify_one();; // notify the waiting thread
    

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
        std::lock_guard lock(global_task_mutex);
        global_task_queue.push(std::move(task));
    }
   
    global_task_cv.notify_one();; // notify the waiting thread
    return done.get();
}


bool PydrofoilCore::remove_breakpoint(vcml::u64 addr) 
{
    PythonTask task;
    task.py_funct = Funct::RemoveBrkp;
    task.arg = addr;
    task.caller_core = this;
    std::future<uint64_t> done = task.result.get_future();

    {
        std::lock_guard lock(global_task_mutex);
        global_task_queue.push(std::move(task));
    }
   
    global_task_cv.notify_one();; // notify the waiting thread
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
    PythonTask task;
    task.py_funct = Funct::SetHartId;
    task.arg = m_hart_id; // Use the member variable we saved
    task.caller_core = this;
    std::cout << "DEBUG: Calling set_hartid in reset with hart_id:----------------------------- " << m_hart_id << std::endl;

    std::future<uint64_t> done = task.result.get_future();
    {
        std::lock_guard lock(global_task_mutex);
        global_task_queue.push(std::move(task));
    }
    global_task_cv.notify_one();;
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
    global_task_cv.notify_one();; // notify the waiting thread

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
    task.caller_core = this;
    std::future<uint64_t> done = task.result.get_future();

    {
        std::lock_guard lock(global_task_mutex);
        global_task_queue.push(std::move(task));
    }
    global_task_cv.notify_one();; // notify the waiting thread

    done.get(); // Wait for the result
}

void PydrofoilCore::global_python_worker_loop() {
    // Da create_handlers() kein Argument mehr braucht, rufen wir es einfach auf
    std::unordered_map<Funct, std::function<void(PythonTask&)>> handlers = create_handlers();

    while(true) {
        PythonTask task;

        {   
            std::unique_lock<std::mutex> lock(global_task_mutex); 
            // Warte, bis die globale Queue nicht leer ist ODER der Worker gestoppt werden soll
            global_task_cv.wait(lock, []{ return !global_task_queue.empty() || !global_worker_running; });
            if (!global_worker_running && global_task_queue.empty())
                break;
            
            task = std::move(global_task_queue.front()); 
            global_task_queue.pop();                      
        } 

        auto it = handlers.find(task.py_funct);
        if(it != handlers.end()) {
            it->second(task); // Hier wird das entsprechende Lambda aufgerufen!
        }
    }
}


void PydrofoilCore::end_of_elaboration()
{
    processor::end_of_elaboration();

    PythonTask task;
    task.py_funct = Funct::SetCb;
    task.caller_core = this;
    std::future<uint64_t> done = task.result.get_future();
    

    {
        std::lock_guard lock(global_task_mutex);
        global_task_queue.push(std::move(task));
    }
    global_task_cv.notify_one();; // notify the waiting thread
    done.get(); // Wait for the result
}