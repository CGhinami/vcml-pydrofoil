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
#include <systemc>
#include "python_tasks.h"
#include "mailbox.h"
#include <unordered_map>
#include "arch.h"
#include <atomic>

// FOrward declaration
namespace backend {
struct PythonTask;
}

namespace core {

enum : size_t {
    MEIP = 0,     // irq for machine-level external interrupts
    SEIP = 1,     // irq for supervisor-level external interrupts
    MTIP = 2,     // irq for machine timer interrupt pending
    MSIP = 3,     // irq for machine software interrupt pending
    NIRQ_LINES = 4
};

enum : size_t {
    MEIP_BIT = 11, // interrupt-pending bit for machine-level external interrupts
    SEIP_BIT = 9,   // interrupt-pending bit for supervisor-level external interrupts
    MTIP_BIT = 7,
    MSIP_BIT = 3
};

// Lookup array
static constexpr std::array<uint32_t, NIRQ_LINES> irq_to_mip_bit = {MEIP_BIT, SEIP_BIT, MTIP_BIT, MSIP_BIT};

class PydrofoilCore : public vcml::processor {
    public:
    vcml::property<std::string> elf;
    vcml::property<std::string> arch_name;
    vcml::property<bool> verbosity;
    // Where the model's HTIF tohost/fromhost window sits. Left at 0 the
    // model keeps its own default (0x80001000)
    vcml::property<vcml::u64> htif_tohost;
    // "path:0xLO:0xLEN" --> write that span of guest physical memory
    // A guest that patches its own text has to be lifted from what it settled on
    vcml::property<std::string> mem_dump;

    PydrofoilCore(const sc_core::sc_module_name& name);
    virtual ~PydrofoilCore();

    void* cpu;

    bool use_dmi;
    tlm::tlm_dmi dmi_cache;
    unsigned long int n_cycles;

    // The total number of external interrupt inputs the PLIC can accept
    // vcml::gpio_target_array<vcml::riscv::plic::NIRQ> irq;
    std::atomic<uint64_t> irq_lines{0};

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
        void* dest;            // for reads
        uint64_t value;        // for writes
        bool success = false;  // written by the consumer (SystemC thread) once serviced
    };
    backend::Mailbox<MemAccess> memtask_mailbox;

    architecture::Model core_arch;

    void set_insns_tick(vcml::u64 val);

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

    private:
    bool step;
    uint64_t insns_per_tick;

    // Latest requested level per interrupt line, applied at the top of the
    // next simulate(). One slot per line rather than a single pending
    // request: MEIP and SEIP can both change within a quantum, and the
    // previous single-slot version dropped whichever arrived first.
    // std::optional<bool> pending_irq[NIRQ_LINES];

    //void notify_pending_irq(size_t irq, bool set);

    std::thread python_worker_thread;
    mutable backend::Mailbox<backend::PythonTask> task_mailbox; // mutable: relax const-correctness for
                                                                 // callers like read_reg_dbg()
    bool stop_worker;

    void set_verbosity(bool value);
    void python_worker_loop();
    void test_reg_access(size_t regno);

    bool check_htif_done();
    void handle_guest_exit();
    void handle_breakpoint_hit();

    void dump_guest_memory();

    protected:
    virtual void before_end_of_elaboration() override;
    virtual void end_of_simulation() override;
};

} // namespace core

#endif
