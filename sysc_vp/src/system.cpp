#include "system.h"

namespace virtual_platform {

system::system(const sc_core::sc_module_name &nm)
    : vcml::system(nm),
      ram("ram", {SRAM_LO, SRAM_HI}),
      bram("bram", {BOOT_LO, BOOT_HI}),
      addr_uart0("addr_uart0", {UART0_LO, UART0_HI}),
      addr_plic("addr_plic", {PLIC_LO, PLIC_HI}),
      addr_clint("addr_clint", {CLINT_LO, CLINT_HI}),
      addr_simdev("addr_simdev", {SIMDEV_LO, SIMDEV_HI}),
      addr_multicore_simdev("addr_multicore_simdev", {MULTICORE_SIMDEV_LO, MULTICORE_SIMDEV_HI}),
      irq_uart0("irq_uart0", IRQ_UART0),
      
      // --- Explizite Initialisierung ---
      m_core0("core0", 0),
#if NRCPU > 1
      m_core1("core1", 1),
#endif
#if NRCPU > 2
      m_core2("core2", 2),
      m_core3("core3", 3),
#endif
#if NRCPU > 4
      m_core4("core4", 4),
      m_core5("core5", 5),
      m_core6("core6", 6),
      m_core7("core7", 7),
#endif

      m_bus("bus"),
      m_ram("sram", ram.get().length()),
      m_bram("bram", bram.get().length()),
      m_throttle("throttle"),
      m_loader("loader"),
      m_clock_cpu("clk_cpu", 16 * vcml::MHz),
      m_reset("rst"),
      m_uart0("uart0"),
      m_plic("plic"),
      m_clint("clint"),
      m_simdev("simdev"),
      m_term("term"),
      m_multicore_simdev("multicore_simdev", NRCPU)
{
  tlm_bind(m_bus, m_loader, "insn");
  tlm_bind(m_bus, m_loader, "data");
  tlm_bind(m_bus, m_ram, "in", ram);
  tlm_bind(m_bus, m_bram, "in", bram);
  tlm_bind(m_bus, m_plic, "in", addr_plic);
  tlm_bind(m_bus, m_uart0, "in", addr_uart0);
  tlm_bind(m_bus, m_simdev, "in", addr_simdev);
  tlm_bind(m_bus, m_multicore_simdev, "in", addr_multicore_simdev);
  tlm_bind(m_bus, m_clint, "in", addr_clint);

  clk_bind(m_clock_cpu, "clk", m_ram, "clk");
  clk_bind(m_clock_cpu, "clk", m_bram, "clk");
  clk_bind(m_clock_cpu, "clk", m_bus, "clk");
  clk_bind(m_clock_cpu, "clk", m_loader, "clk");
  clk_bind(m_clock_cpu, "clk", m_plic, "clk");
  clk_bind(m_clock_cpu, "clk", m_uart0, "clk");
  clk_bind(m_clock_cpu, "clk", m_simdev, "clk");
  clk_bind(m_clock_cpu, "clk", m_multicore_simdev, "clk");
  clk_bind(m_clock_cpu, "clk", m_clint, "clk");

  gpio_bind(m_reset, "rst", m_bus, "rst");
  gpio_bind(m_reset, "rst", m_ram, "rst");
  gpio_bind(m_reset, "rst", m_bram, "rst");
  gpio_bind(m_reset, "rst", m_loader, "rst");
  gpio_bind(m_reset, "rst", m_plic, "rst");
  gpio_bind(m_reset, "rst", m_uart0, "rst");
  gpio_bind(m_reset, "rst", m_simdev, "rst");
  gpio_bind(m_reset, "rst", m_multicore_simdev, "rst");
  gpio_bind(m_uart0, "irq", m_plic, "irqs", IRQ_UART0);
  gpio_bind(m_reset, "rst", m_clint, "rst");

  serial_bind(m_term, "serial_tx", m_uart0, "serial_rx");
  serial_bind(m_term, "serial_rx", m_uart0, "serial_tx");

  // --- Explizites Binding ---

  // CORE 0
  tlm_bind(m_bus, m_core0, "insn");
  tlm_bind(m_bus, m_core0, "data");
  clk_bind(m_clock_cpu, "clk", m_core0, "clk");
  gpio_bind(m_reset, "rst", m_core0, "rst");
  m_plic.irqt[0].bind(m_core0.irq[0]);
  m_clint.irq_sw[0].bind(m_core0.irq[core::MSIP]);
  m_clint.irq_timer[0].bind(m_core0.irq[core::MTIP]);

#if NRCPU > 1
  // CORE 1
  tlm_bind(m_bus, m_core1, "insn");
  tlm_bind(m_bus, m_core1, "data");
  clk_bind(m_clock_cpu, "clk", m_core1, "clk");
  gpio_bind(m_reset, "rst", m_core1, "rst");
  m_plic.irqt[1].bind(m_core1.irq[0]);
  m_clint.irq_sw[1].bind(m_core1.irq[core::MSIP]);
  m_clint.irq_timer[1].bind(m_core1.irq[core::MTIP]);
#endif

#if NRCPU > 2
  // CORE 2
  tlm_bind(m_bus, m_core2, "insn");
  tlm_bind(m_bus, m_core2, "data");
  clk_bind(m_clock_cpu, "clk", m_core2, "clk");
  gpio_bind(m_reset, "rst", m_core2, "rst");
  m_plic.irqt[2].bind(m_core2.irq[0]);
  m_clint.irq_sw[2].bind(m_core2.irq[core::MSIP]);
  m_clint.irq_timer[2].bind(m_core2.irq[core::MTIP]);

  // CORE 3
  tlm_bind(m_bus, m_core3, "insn");
  tlm_bind(m_bus, m_core3, "data");
  clk_bind(m_clock_cpu, "clk", m_core3, "clk");
  gpio_bind(m_reset, "rst", m_core3, "rst");
  m_plic.irqt[3].bind(m_core3.irq[0]);
  m_clint.irq_sw[3].bind(m_core3.irq[core::MSIP]);
  m_clint.irq_timer[3].bind(m_core3.irq[core::MTIP]);
#endif

#if NRCPU > 4
  // CORE 4
  tlm_bind(m_bus, m_core4, "insn"); tlm_bind(m_bus, m_core4, "data");
  clk_bind(m_clock_cpu, "clk", m_core4, "clk"); gpio_bind(m_reset, "rst", m_core4, "rst");
  m_plic.irqt[4].bind(m_core4.irq[0]); m_clint.irq_sw[4].bind(m_core4.irq[core::MSIP]); m_clint.irq_timer[4].bind(m_core4.irq[core::MTIP]);

  // CORE 5
  tlm_bind(m_bus, m_core5, "insn"); tlm_bind(m_bus, m_core5, "data");
  clk_bind(m_clock_cpu, "clk", m_core5, "clk"); gpio_bind(m_reset, "rst", m_core5, "rst");
  m_plic.irqt[5].bind(m_core5.irq[0]); m_clint.irq_sw[5].bind(m_core5.irq[core::MSIP]); m_clint.irq_timer[5].bind(m_core5.irq[core::MTIP]);

  // CORE 6
  tlm_bind(m_bus, m_core6, "insn"); tlm_bind(m_bus, m_core6, "data");
  clk_bind(m_clock_cpu, "clk", m_core6, "clk"); gpio_bind(m_reset, "rst", m_core6, "rst");
  m_plic.irqt[6].bind(m_core6.irq[0]); m_clint.irq_sw[6].bind(m_core6.irq[core::MSIP]); m_clint.irq_timer[6].bind(m_core6.irq[core::MTIP]);

  // CORE 7
  tlm_bind(m_bus, m_core7, "insn"); tlm_bind(m_bus, m_core7, "data");
  clk_bind(m_clock_cpu, "clk", m_core7, "clk"); gpio_bind(m_reset, "rst", m_core7, "rst");
  m_plic.irqt[7].bind(m_core7.irq[0]); m_clint.irq_sw[7].bind(m_core7.irq[core::MSIP]); m_clint.irq_timer[7].bind(m_core7.irq[core::MTIP]);
#endif
}

system::~system()
{
  std::error_code ec;
  std::filesystem::remove_all("/tmp/isolated_libs", ec);
  if (ec) {
      mwr::log_warn("Failed to clean up isolated_libs: %s", ec.message().c_str());
  } else {
      mwr::log_info("Cleaned up entire isolated_libs directory.");
  }
}

int system::run()
{
  double simstart = mwr::timestamp();
  int result = vcml::system::run();
  double realtime = mwr::timestamp() - m_multicore_simdev.last_queried_time;
  double duration = sc_core::sc_time_stamp().to_seconds();

  // --- Explizites Aufaddieren der Zyklen ---
  vcml::u64 ninsn = m_core0.cycle_count();
#if NRCPU > 1
  ninsn += m_core1.cycle_count();
#endif
#if NRCPU > 2
  ninsn += m_core2.cycle_count() + m_core3.cycle_count();
#endif
#if NRCPU > 4
  ninsn += m_core4.cycle_count() + m_core5.cycle_count() + m_core6.cycle_count() + m_core7.cycle_count();
#endif

  double mips = realtime == 0.0 ? 0.0 : ninsn / realtime / 1e6;
  vcml::log_info("total");
  vcml::log_info("  duration       : %.9fs", duration);
  vcml::log_info("  runtime        : %.4fs", realtime);
  vcml::log_info("  cycles core 0  : %llu", m_core0.cycle_count());
#if NRCPU > 1
  vcml::log_info("  cycles core 1  : %llu", m_core1.cycle_count());
#endif
#if NRCPU > 2
  vcml::log_info("  cycles core 2  : %llu", m_core2.cycle_count());
  vcml::log_info("  cycles core 3  : %llu", m_core3.cycle_count());
#endif
#if NRCPU > 4
  vcml::log_info("  cycles core 4  : %llu", m_core4.cycle_count());
  vcml::log_info("  cycles core 5  : %llu", m_core5.cycle_count());
  vcml::log_info("  cycles core 6  : %llu", m_core6.cycle_count());
  vcml::log_info("  cycles core 7  : %llu", m_core7.cycle_count());
#endif
  vcml::log_info("  sim speed      : %.1f MIPS", mips);
  vcml::log_info("  realtime ratio : %.2f / 1s", realtime == 0.0 ? 0.0 : realtime / duration);

  return result;
}

} // namespace virtual_platform