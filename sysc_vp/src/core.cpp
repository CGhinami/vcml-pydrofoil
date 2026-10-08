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
#include <cstring>
#include "riscv_arch.h"

namespace core {

PydrofoilCore::PydrofoilCore(const sc_core::sc_module_name& name, uint64_t hart_id):
    vcml::processor(name, "riscv"),
    elf("elf", ""),
    arch_name("arch_name", "rv64"),
    verbosity("verbose", false),
    htif_tohost("htif_tohost", 0),
    mem_dump("mem_dump", ""),
    pydrofoil_lib("pydrofoil_lib", PYDROFOIL_CAPI_LIB),
    hart_id(hart_id),
    cpu(nullptr),
    use_dmi(true),
    n_cycles(0),
    step(true), // For the first execution we want just 1 instruction to run
    insns_per_tick(0),
    stop_worker(false),
    core_arch(arch_name.c_str(), arch_name == "rv64" ? 64 : 32, architecture::regdb_riscv, 33)
{
    mwr::log_info("Running with arch: %d bit", 8 * core_arch.word_size());
    set_little_endian(); // Otherwise the gdbserver inverts the bytes it reads

    std::string err = lib.load(pydrofoil_lib.get());
    VCML_ERROR_ON(!err.empty(), "hart %lu: %s", (unsigned long)hart_id, err.c_str());

    python_worker_thread = std::thread(&PydrofoilCore::python_worker_loop, this);

    backend::PythonTask task;
    task.py_funct = backend::Funct::Init;
    task.arg = arch_name;
    task_mailbox.submit(task); // blocks (spinning) until the worker thread has handled it

    set_verbosity(verbosity.get());

    for(size_t i = 0; i < core_arch.reg_number(); ++i)
        define_cpureg_rw(i, core_arch.get_regs_ptr()[i].gdb_name, core_arch.word_size());
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
    // normally already done in end_of_simulation()
    finish_inflight_simulate();

    if(cpu) {
        backend::PythonTask task;
        task.py_funct = backend::Funct::FreeCpu;

        // must be visible to the worker before it observes the FreeCpu
        // request: sequenced-before task_mailbox.submit()'s internal post(),
        // which release-publishes it, matching the acquire in the worker's
        // take() -- see python_worker_loop().
        stop_worker = true;
        task_mailbox.submit(task);
    }

    python_worker_thread.join();
}

// void PydrofoilCore::notify_pending_irq(size_t irq, bool set)
// {
//     uint32_t bit = irq_to_mip_bit[irq];

//     backend::PythonTask task;
//     task.py_funct = backend::Funct::SetMIP;
//     task.arg = backend::IrqArgs{ bit, set };
//     task_mailbox.submit(task);
// }

void PydrofoilCore::interrupt(size_t irq, bool set)
{
    if(irq >= NIRQ_LINES)
        return;

    // Watch out, 1 by definition is a signed int
    const uint64_t mask = uint64_t{1} << irq_to_mip_bit[irq];

    // fetch or/and ensure atomic operations!
    if(set)
        irq_lines.fetch_or(mask, std::memory_order_release);
    else
        irq_lines.fetch_and(~mask, std::memory_order_release);
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

    task_mailbox.submit(task);
    return task.result;
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
    task_mailbox.submit(task);

    uint64_t reg_val = task.result;
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
            mwr::log_info("DMI start: %lx, DMI end: %lx", s, e);

            backend::PythonTask task;
            task.py_funct = backend::Funct::SetDMI;
            task.arg = s;
            task_mailbox.submit(task);
            if(task.result != 0)
                mwr::log_info("Setting DMI pointer failed");
        }
    }
}

// Called from a coroutine
void PydrofoilCore::simulate(size_t cycles)
{
    // for(size_t irq = 0; irq < NIRQ_LINES; ++irq) {
    //     if(pending_irq[irq].has_value()) {
    //         notify_pending_irq(irq, pending_irq[irq].value());
    //         pending_irq[irq].reset();
    //     }
    // }

    backend::PythonTask task;
    task.py_funct = backend::Funct::Simulate;
    task.arg = step ? 1 : cycles;

    // post(), not submit(): we can't just spin-wait for the response here,
    // because while the worker thread is inside pydrofoil_cpu_simulate() it
    // may need us to service a non-DMI memory access via memtask_mailbox --
    // and that's *this* thread's job (see memory_callbacks.cpp). So this
    // loop multiplexes "is the Simulate task done yet" against "does the
    // worker need a memory access serviced", spinning on both mailboxes.
    task_mailbox.post(task);
    simulate_in_flight = &task;

    try {
        int spins = 0;
        while(!task_mailbox.is_done()) {
            MemAccess* memtask = memtask_mailbox.try_take();
            if(memtask == nullptr) {
                backend::relax(spins);
                continue;
            }
            spins = 0;

            bool success = service_memtask(*memtask);
            if(!success)
                mwr::log_info("Memory access failed with address: %lx", memtask->addr);

            memtask->success = success;
            memtask_mailbox.complete();
        }
    } catch(...) {
        // async=true: sc_sync() throws once the simulation has ended. The
        // worker is still inside the Simulate task and will write into
        // 'task', so let it finish before this frame is unwound.
        finish_inflight_simulate();
        throw;
    }
    task_mailbox.wait_done(); // already done; this just resets the mailbox for the next call
    simulate_in_flight = nullptr;

    size_t current_steps = task.result;
    bool early_return = current_steps > 0 && current_steps < cycles;

    // htif_done has to be checked unconditionally: unlike a breakpoint hit,
    // guest exit doesn't reliably make current_steps land below cycles (it
    // depends on where inside the quantum's tick-batching the exit-triggering
    // write happens to fall), so it can't be inferred from early_return.
    if(!step && check_htif_done())
        handle_guest_exit();
    else if(!step && early_return)
        handle_breakpoint_hit();

    n_cycles += current_steps;
    check_for_dmi_regions();
    step = false;
}

// With async=true simulate() runs on vcml's async thread, which must not
// drive TLM transactions itself (peripherals may wait() on them): sc_sync()
// hands the access to the SystemC thread and blocks until it is done. On the
// SystemC thread (async=false) it is done directly.
bool PydrofoilCore::service_memtask(MemAccess& memtask)
{
    bool success = false;
    auto access = [&]() {
        if(memtask.type == MemTask::Read)
            success = data.read(memtask.addr, memtask.dest, memtask.size, vcml::SBI_NONE) == tlm::TLM_OK_RESPONSE;
        else
            success = data.write(memtask.addr, &memtask.value, memtask.size, vcml::SBI_NONE) == tlm::TLM_OK_RESPONSE;
    };

    if(vcml::sc_is_async())
        vcml::sc_sync(access);
    else
        access();
    return success;
}

bool PydrofoilCore::check_htif_done()
{
    backend::PythonTask task;
    task.py_funct = backend::Funct::GetExit;
    task_mailbox.submit(task);
    return task.result != 0;
}

void PydrofoilCore::handle_guest_exit()
{
    mwr::log_info("Stop requested");
    // sc_stop() may only be called from the SystemC thread
    if(vcml::sc_is_async())
        vcml::sc_sync([]() { vcml::request_stop(); });
    else
        vcml::request_stop();
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
    backend::PythonTask task;
    task.py_funct = backend::Funct::SetBrkp;
    task.arg = addr;
    task_mailbox.submit(task);
    return task.result;
}

bool PydrofoilCore::remove_breakpoint(vcml::u64 addr)
{
    backend::PythonTask task;
    task.py_funct = backend::Funct::RemoveBrkp;
    task.arg = addr;
    task_mailbox.submit(task);
    return task.result;
}

// Called from a coroutine
vcml::u64 PydrofoilCore::cycle_count() const
{
    return n_cycles;
}

void PydrofoilCore::reset()
{
    // pydrofoil_cpu_reset(cpu);
}

void PydrofoilCore::set_insns_tick(vcml::u64 val)
{
    insns_per_tick = val;
}

// NOTE: the std::future/std::promise + mutex/condition_variable handoff this
// file used to have (and the shared_ptr-based sketch that used to live here
// as a "faster but error prone" alternative) has been replaced by
// backend::Mailbox (see mailbox.h): a lock-free single-slot rendezvous. It
// gets the speed of the sketch below without its problems -- no shared_ptr
// lifetime juggling, no separate done_mutex/done_cv per task, still exactly
// one thread ever calls into pydrofoil.

void PydrofoilCore::set_verbosity(bool value)
{
    backend::PythonTask task;
    task.py_funct = backend::Funct::SetVerbosity;
    task.arg = (uint32_t) value;
    task_mailbox.submit(task);
}

// The reason why we need a second thread is that it's important to avoid breaking RPython mrmory tracking
// Now all the actions that call python functions will be in one single thread
void PydrofoilCore::python_worker_loop()
{
    std::unordered_map<backend::Funct, std::function<void(backend::PythonTask&)>> handlers = backend::create_handlers(
        *this);

    while(true) {
        backend::PythonTask& task = task_mailbox.take(); // spins until the caller side posts a request

        auto it = handlers.find(task.py_funct);
        if(it != handlers.end())
            it->second(task);

        task_mailbox.complete(); // unblocks the caller's wait_done()/is_done()

        // stop_worker is set (by ~PydrofoilCore()) strictly before that
        // final FreeCpu request is posted, so it's already visible to us
        // here -- handle the task first (FreeCpu still has to run), then
        // exit.
        if(stop_worker)
            break;
    }
}

// Write out a span of guest physical memory, reading it straight from the
// DMI window the platform already granted pydrofoil. Going through the bus
// instead would work too, but this runs after the simulation has stopped,
// where issuing TLM transactions is the more surprising of the two.
void PydrofoilCore::dump_guest_memory()
{
    const std::string spec = mem_dump.get();
    if(spec.empty())
        return;

    size_t c1 = spec.find(':');
    size_t c2 = (c1 == std::string::npos) ? std::string::npos : spec.find(':', c1 + 1);
    if(c1 == std::string::npos || c2 == std::string::npos) {
        mwr::log_warn("mem_dump: expected \"path:0xLO:0xLEN\", got \"%s\"", spec.c_str());
        return;
    }

    const std::string path = spec.substr(0, c1);
    const uint64_t lo = std::stoull(spec.substr(c1 + 1, c2 - c1 - 1), nullptr, 0);
    const uint64_t len = std::stoull(spec.substr(c2 + 1), nullptr, 0);

    for(const auto& entry : mem_regions) {
        const MemRegion& r = entry.second;
        if(lo < r.start_addr || lo + len > r.start_addr + r.size)
            continue;

        FILE* f = std::fopen(path.c_str(), "wb");
        if(f == nullptr) {
            mwr::log_warn("mem_dump: cannot open '%s'", path.c_str());
            return;
        }
        std::fwrite(r.ptr + (lo - r.start_addr), 1, len, f);
        std::fclose(f);
        mwr::log_info("mem_dump: wrote 0x%lx bytes from 0x%lx to '%s'", (unsigned long)len,
                      (unsigned long)lo, path.c_str());
        return;
    }

    mwr::log_warn("mem_dump: no DMI region covers 0x%lx..0x%lx", (unsigned long)lo,
                  (unsigned long)(lo + len));
}

// The simulation can end while the worker is still inside a Simulate task:
// sc_stop() requested mid-quantum (e.g. by a peripheral) leaves the SystemC
// thread that services the worker's memory accesses suspended in a wait()
// (e.g. the CLINT's sync on mtime) that never returns. Answer the remaining
// accesses here (reads see 0) until the worker finishes the quantum,
// otherwise it never picks up another task and we deadlock. Must run while
// the RAM still exists, the worker keeps reading it through DMI.
void PydrofoilCore::finish_inflight_simulate()
{
    if(!simulate_in_flight)
        return;

    while(!task_mailbox.is_done()) {
        MemAccess* memtask = memtask_mailbox.try_take();
        if(memtask == nullptr)
            continue;
        if(memtask->type == MemTask::Read)
            std::memset(memtask->dest, 0, memtask->size);
        memtask->success = true;
        memtask_mailbox.complete();
    }
    task_mailbox.wait_done();

    // simulate() never got to count this quantum
    n_cycles += simulate_in_flight->result;
    simulate_in_flight = nullptr;
}

void PydrofoilCore::end_of_simulation()
{
    // async=true: simulate() runs on vcml's async thread, join it before
    // touching the mailboxes from here (it finishes or drains its own quantum)
    if(async)
        vcml::sc_join_async();
    finish_inflight_simulate();
    dump_guest_memory();
    processor::end_of_simulation();
}

void PydrofoilCore::before_end_of_elaboration()
{
    // Installing the memory callbacks rebuilds the machine object, and a
    // fresh machine has freshly reset registers. processor's own
    // before_end_of_elaboration() is what flushes the cpureg properties
    // (system.<core>.pc, .a0, .a1, ...) into the model, so the callbacks
    // have to be in place *first* -- otherwise every register the
    // configuration asks for is silently discarded a moment later. That
    // went unnoticed while the only image in use started at the model's
    // own reset PC, where the wipe is a no-op.
    backend::PythonTask task;
    task.py_funct = backend::Funct::SetCb;
    task_mailbox.submit(task);

    // Same reasoning, and it must also precede any guest execution: the
    // model consults tohost on every access, not just at startup.
    if(htif_tohost.get() != 0) {
        backend::PythonTask htif_task;
        htif_task.py_funct = backend::Funct::SetHtifTohost;
        htif_task.arg = (uint64_t)htif_tohost.get();
        task_mailbox.submit(htif_task);
        mwr::log_info("HTIF tohost placed at 0x%lx", (unsigned long)htif_tohost.get());
    }

    task.py_funct = backend::Funct::SetExtClint;
    task.arg = uint64_t{1};
    task_mailbox.submit(task);

    task.py_funct = backend::Funct::SetIrqLines;
    task.arg = reinterpret_cast<uint64_t*>(&irq_lines);
    task_mailbox.submit(task);

    task.py_funct = backend::Funct::SetTickFreq;
    task.arg = insns_per_tick;
    task_mailbox.submit(task);

    task.py_funct = backend::Funct::SetHartId;
    task.arg = hart_id;
    task_mailbox.submit(task);
    VCML_ERROR_ON(task.result != 0, "hart %lu: setting mhartid failed", (unsigned long)hart_id);

    processor::before_end_of_elaboration();
}

} // namespace core
