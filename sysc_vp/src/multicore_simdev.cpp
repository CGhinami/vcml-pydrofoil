#include "multicore_simdev.h"

namespace vcml {
namespace meta {

void multicore_simdev::write_core_done(u32 val) {
    if (val >= 64) {
        log_warn("Core ID %u is out of bounds (max 63)", val);
        return;
    }

    m_done_mask |= (1ULL << val);
    u64 target_mask = (1ULL << m_num_cores) - 1;

    log_info("Core %u has finished (Current Mask: 0x%llx / Target Mask: 0x%llx)", val, m_done_mask, target_mask);

    if (m_done_mask == target_mask) {
        log_info("All %u cores have finished! Stopping simulation.", m_num_cores);
        request_stop(); 
    }
}

// <-- NEU: Schreibt in die Datei von Core 0
void multicore_simdev::write_sout0(u32 val) {
    char c = (char)(val & 0xFF);
    if (m_out_core0.is_open()) {
        m_out_core0 << c;
        m_out_core0.flush(); // Sofort auf Festplatte schreiben
    } else {
        fputc(c, stdout);
        fflush(stdout);
    }
}

// <-- NEU: Schreibt in die Datei von Core 1
void multicore_simdev::write_sout1(u32 val) {
    char c = (char)(val & 0xFF);
    if (m_out_core1.is_open()) {
        m_out_core1 << c;
        m_out_core1.flush(); // Sofort auf Festplatte schreiben
    } else {
        fputc(c, stdout);
        fflush(stdout);
    }
}

multicore_simdev::multicore_simdev(const sc_module_name& nm, unsigned int num_cores):
    peripheral(nm),
    m_done_mask(0),
    m_num_cores(num_cores), 
    core_done("core_done", 0x00, 0),
    sout0("sout0", 0x08, 0), // <-- Initialisiere Core 0 Output
    sout1("sout1", 0x0C, 0), // <-- Initialisiere Core 1 Output (4 Byte weiter!)
    in("in") 
{
    // Configure registers
    core_done.allow_read_write();
    core_done.on_write(&multicore_simdev::write_core_done);

    sout0.allow_read_write();
    sout0.on_write(&multicore_simdev::write_sout0);

    sout1.allow_read_write();
    sout1.on_write(&multicore_simdev::write_sout1);

    // <-- NEU: Öffne die beiden Textdateien
    // std::ios::trunc sorgt dafür, dass alte Logs bei Neustart gelöscht werden
    m_out_core0.open("core0_output.txt", std::ios::out | std::ios::trunc);
    m_out_core1.open("core1_output.txt", std::ios::out | std::ios::trunc);
}

multicore_simdev::~multicore_simdev() {
    // <-- NEU: Dateien sauber schließen
    if (m_out_core0.is_open()) m_out_core0.close();
    if (m_out_core1.is_open()) m_out_core1.close();
}

void multicore_simdev::reset() {
    m_done_mask = 0; 
}

} // namespace meta
} // namespace vcml