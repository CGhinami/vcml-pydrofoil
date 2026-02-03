#include "system.h"

system::system(const sc_core::sc_module_name &nm)
    : vcml::system(nm), 
    ram("ram", {SRAM_LO, SRAM_HI}),
    bram("bram", {BOOT_LO, BOOT_HI}),
    m_core("core", "rv64", 0),
    m_core2("core2", "rv64", 1),
    m_bus("bus"),
    m_ram("sram", ram.get().length()),
    m_bram("bram", bram.get().length()),
    m_throttle("throttle"),
    m_loader("loader"),
    m_clock_cpu("clk_cpu", 16 * vcml::MHz),
    m_reset("rst") {

    tlm_bind(m_bus, m_loader, "insn");
    tlm_bind(m_bus, m_loader, "data");
    tlm_bind(m_bus, m_ram, "in", ram);
    tlm_bind(m_bus, m_bram, "in", bram);

    tlm_bind(m_bus, m_core, "insn");
    tlm_bind(m_bus, m_core, "data");
    
    tlm_bind(m_bus, m_core2, "insn");
    tlm_bind(m_bus, m_core2, "data");

    clk_bind(m_clock_cpu, "clk", m_core, "clk");
    clk_bind(m_clock_cpu, "clk", m_core2, "clk");
    clk_bind(m_clock_cpu, "clk", m_ram, "clk");
    clk_bind(m_clock_cpu, "clk", m_bram, "clk");
    clk_bind(m_clock_cpu, "clk", m_bus, "clk");
    clk_bind(m_clock_cpu, "clk", m_loader, "clk");


    gpio_bind(m_reset, "rst", m_core, "rst");
    gpio_bind(m_reset, "rst", m_core2, "rst");
    gpio_bind(m_reset, "rst", m_bus, "rst");
    gpio_bind(m_reset, "rst", m_ram, "rst");
    gpio_bind(m_reset, "rst", m_bram, "rst");
    gpio_bind(m_reset, "rst", m_loader, "rst");

}

system::~system() {
  // nothing to do
}

int system::run() {
    double simstart = mwr::timestamp();
    int result = vcml::system::run();
    double realtime = mwr::timestamp() - simstart;
    double duration = sc_core::sc_time_stamp().to_seconds();
    vcml::u64 ninsn = m_core.cycle_count();
    vcml::u64 ninsn2 = m_core2.cycle_count();
    double mips = realtime == 0.0 ? 0.0 : ninsn / realtime / 1e6;
    vcml::log_info("total");
    vcml::log_info("  duration       : %.9fs", duration);
    vcml::log_info("  runtime        : %.4fs", realtime);
    vcml::log_info("  instructions   : %llu", ninsn);
    vcml::log_info("  instructions2  : %llu", ninsn2);
    vcml::log_info("  total instructions: %llu", ninsn + ninsn2);
    vcml::log_info("  sim speed      : %.1f MIPS", mips);
    vcml::log_info("  realtime ratio : %.2f / 1s",
                   realtime == 0.0 ? 0.0 : realtime / duration);

    return result;
}
