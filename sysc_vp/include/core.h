#ifndef CORE_H
#define CORE_H

#include "vcml.h"
#include <future>
#include <variant>
#include <systemc>
#include "python_tasks.h"
#include <unordered_map>
#include "arch.h"

enum : size_t {
    MEIP = 0, //irq for machine-level external interrupts
    SEIP = 1  //irq for supervisor-level external interrupts
};

enum : size_t {
    MEIP_BIT = 11, //interrupt-pending bit for machine-level external interrupts
    SEIP_BIT = 9   //interrupt-pending bit for supervisor-level external interrupts
};



struct PythonTask;
class PydrofoilCore : public vcml::processor{
    public:
        // --- ADDED FOR DLOPEN ---
        void* m_pydrofoil_handle; // Stores the open library
        
        // Define the shape of the functions
        
        // Create the actual function pointers
        void* (*m_pydrofoil_allocate_cpu)(const char*, const char*);
        int (*m_pydrofoil_cpu_set_ram_read_write_callback)(void *, int(*)(void *, uint64_t, int, void *, void *), int(*)(void *, uint64_t, int, uint64_t, void *), void *);
        uint64_t (*m_pydrofoil_cpu_cycles)(void *);
        int (*m_pydrofoil_cpu_set_breakpoint)(void *, uint64_t);
        int (*m_pydrofoil_cpu_remove_breakpoint)(void *, uint64_t);
        int (*m_pydrofoil_cpu_simulate)(void *, size_t);
        int (*m_pydrofoil_cpu_write_reg)(void *, char const *, uint64_t);
        uint64_t (*m_pydrofoil_cpu_read_reg)(void *, char const *);
        int (*m_pydrofoil_free_cpu)(void *);
        int (*m_pydrofoil_cpu_set_verbosity)(void *, int);
        int (*m_pydrofoil_cpu_set_dma_region)(void *, uint64_t, uint64_t, uint8_t *);
        int (*m_pydrofoil_set_interrupt_pending)(void *, uint32_t);
        vcml::property<std::string> elf;
        vcml::property<std::string> arch_name;
        vcml::property<bool> verbosity;

        PydrofoilCore(const sc_core::sc_module_name& name);
        ~PydrofoilCore();

        void* cpu;

        bool use_dmi;
        tlm::tlm_dmi dmi_cache;
        unsigned long int n_cycles = 0;

        //The total number of external interrupt inputs the PLIC can accept
        //vcml::gpio_target_array<vcml::riscv::plic::NIRQ> irq; 

        struct MemRegion {
            uint8_t* ptr;
            uint64_t start_addr;
            uint64_t size;
        };

        std::unordered_map<uint64_t, MemRegion> mem_regions;
        void check_for_dmi_regions();

        enum class MemTask {Read, Write};
        struct MemAccess {
            MemTask type;
            uint64_t addr;
            size_t size;
            void* dest; // for reads
            uint64_t value; // for writes
            std::promise<bool> result;
        };
        std::mutex memtask_mutex;
        std::condition_variable memtask_cv;
        std::queue<MemAccess> memtask_queue;

        Model core_arch;


        // This method gets repeatedly called by the processor class
        // The number of steps/cycles depends on the quantum
        void simulate(size_t cycles) override;
        vcml::u64 cycle_count() const override;
        virtual void interrupt(size_t irq, bool set) override;
        void reset() override;

        bool write_reg_dbg(size_t reg, const void* buf, size_t len) override;
        bool read_reg_dbg(size_t regno, void* buf, size_t len) override;
        bool insert_breakpoint(vcml::u64 addr);
        bool remove_breakpoint(vcml::u64 addr);
        void sysc_memory_thread();

    private:
        bool step = true; // For the first execution we want just 1 instruction to run

        std::optional<bool> is_irq_pending;
        size_t irq_num;

        void notify_pending_irq(bool set);

        std::thread python_worker_thread;
        mutable std::queue<PythonTask> task_queue; // mutable is needed to relax the const-correctness compiler check
                                                   // should only have one element
        mutable std::condition_variable task_cv;
        mutable std::mutex task_mutex;
        bool stop_worker = false;

        void set_verbosity(bool value); 
        void python_worker_loop();
        void test_reg_access(size_t regno);

        void handle_breakpoint_hit();

    protected:
        virtual void end_of_elaboration() override;
};

#endif