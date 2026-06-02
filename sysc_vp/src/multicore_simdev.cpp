#include "multicore_simdev.h"

namespace vcml {
namespace meta {

void multicore_simdev::write_core_done(u32 val) {
    if (val >= 64) {
        log_warn("Core ID %u is out of bounds (max 63)", val);
        return;
    }

    // Set the bit for the core that just reported in
    m_done_mask |= (1ULL << val);

    // Calculate the target mask using our standard C++ variable
    u64 target_mask = (1ULL << m_num_cores) - 1;

    log_info("Core %u has finished (Current Mask: 0x%llx / Target Mask: 0x%llx)", val, m_done_mask, target_mask);

    // If all bits are set...
    if (m_done_mask == target_mask) {
        log_info("All %u cores have finished! Stopping simulation.", m_num_cores);
        request_stop(); // Gracefully terminate the simulation
    }
}

void multicore_simdev::write_sout(u32 val) {
    fputc(val, stdout);
    fflush(stdout);
}

// <-- Updated constructor implementation
multicore_simdev::multicore_simdev(const sc_module_name& nm, unsigned int num_cores):
    peripheral(nm),
    m_done_mask(0),
    m_num_cores(num_cores), // <-- Initialize the member variable
    core_done("core_done", 0x00, 0),
    sout("sout", 0x08, 0),
    in("in") 
{
    // Configure registers
    core_done.allow_read_write();
    core_done.on_write(&multicore_simdev::write_core_done);

    sout.allow_read_write();
    sout.on_write(&multicore_simdev::write_sout);
}

multicore_simdev::~multicore_simdev() {
    // Nothing to clean up
}

void multicore_simdev::reset() {
    m_done_mask = 0; // Reset the mask to 0 on system reset
}

} // namespace meta
} // namespace vcml