/******************************************************************************
 *                                                                            *
 * Copyright 2026 Chiara Ghinami                                              *
 *                                                                            *
 * This software is licensed under the MIT license found in the               *
 * LICENSE file at the root directory of this source tree.                    *
 *                                                                            *
 ******************************************************************************/

#ifndef CORE_H
#define CORE_H

#include "vcml.h"
#include <future>
#include <systemc>
#include "python_tasks.h"
#include <unordered_map>
#include "arch.h"
#include <csetjmp>
#include <exception>

// FOrward declaration
namespace backend {
struct PythonTask;
}

namespace core {

enum : size_t {
    MEIP = 0, // irq for machine-level external interrupts
    SEIP = 1,  // irq for supervisor-level external interrupts
    MSIP = 2, // NEW: irq for machine-level software interrupts (IPIs for SMP!)
    MTIP = 3
};

enum : size_t {
    MEIP_BIT = 11, // interrupt-pending bit for machine-level external interrupts
    SEIP_BIT = 9,   // interrupt-pending bit for supervisor-level external interrupts
    MSIP_BIT = 3,  // NEW: RISC-V standard bit for Machine Software Interrupts
    MTIP_BIT = 7   // NEW: RISC-V standard bit for Machine Timer Interrupts
};

class PydrofoilCore : public vcml::processor {
    public:
    vcml::property<std::string> elf;
    vcml::property<std::string> arch_name;
    vcml::property<bool> verbosity;

    PydrofoilCore(const sc_core::sc_module_name &name, uint64_t hart_id = 0);
    virtual ~PydrofoilCore();

    void* cpu;

    bool use_dmi;
    tlm::tlm_dmi dmi_cache;
    unsigned long int n_cycles;

    // The total number of external interrupt inputs the PLIC can accept
    // vcml::gpio_target_array<vcml::riscv::plic::NIRQ> irq;

    struct MemRegion {
        uint8_t* ptr;
        uint64_t start_addr;
        uint64_t size;
    };

    std::unordered_map<uint64_t, MemRegion> mem_regions;
    void check_for_dmi_regions();

    enum class MemTask { Read, Write };
    struct MemAccess {
        MemTask type;
        uint64_t addr;
        size_t size;
        void* dest;     // for reads
        uint64_t value; // for writes
        std::promise<bool> result;
    };
    std::mutex memtask_mutex;
    std::condition_variable memtask_cv;
    std::queue<MemAccess> memtask_queue;

    architecture::Model core_arch;

    // This method gets repeatedly called by the processor class
    // The number of steps/cycles depends on the quantum
    void simulate(size_t cycles) override;
    vcml::u64 cycle_count() const override;
    virtual void interrupt(size_t irq, bool set) override;
    void reset() override;

    virtual bool write_reg_dbg(size_t reg, const void* buf, size_t len) override;
    virtual bool read_reg_dbg(size_t regno, void* buf, size_t len) override;
    virtual bool insert_breakpoint(vcml::u64 addr) override;
    virtual bool remove_breakpoint(vcml::u64 addr) override;
    static void shutdown_worker();
    uint64_t get_hart_id();
    private:
    bool step;

    std::optional<bool> is_irq_pending;
    size_t irq_num;

    void notify_pending_irq(bool set);

        static std::thread global_python_worker_thread;
        static std::queue<backend::PythonTask> global_task_queue;
        static std::condition_variable global_task_cv;
        static std::mutex global_task_mutex;
        static bool global_worker_running;
        
        // Der Loop ist jetzt auch eine statische Funktion (gehört zur Klasse, nicht zum Objekt)
        static void global_python_worker_loop();
        // mutable std::condition_variable task_cv;


    void set_verbosity(bool value);
    void python_worker_loop();
    void test_reg_access(size_t regno);

    void handle_breakpoint_hit();
    uint64_t m_hart_id;

    protected:
    virtual void end_of_elaboration() override;
};

} // namespace core

#endif
