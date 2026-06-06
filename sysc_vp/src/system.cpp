#include "system.h"
system::system(const sc_core::sc_module_name &nm)
    : vcml::system(nm),
      ram("ram", {SRAM_LO, SRAM_HI}),
      bram("bram", {BOOT_LO, BOOT_HI}),
      addr_uart0("addr_uart0", {UART0_LO, UART0_HI}),
      addr_plic("addr_plic", {PLIC_LO, PLIC_HI}),
      addr_simdev("addr_simdev", {SIMDEV_LO, SIMDEV_HI}),
      addr_multicore_simdev("addr_multicore_simdev", {MULTICORE_SIMDEV_LO, MULTICORE_SIMDEV_HI}),
      irq_uart0("irq_uart0", IRQ_UART0),
      m_core("core", 0),
      m_core2("core2", 1),
      m_bus("bus"),
      m_ram("sram", ram.get().length()),
      m_bram("bram", bram.get().length()),
      m_throttle("throttle"),
      m_loader("loader"),
      m_clock_cpu("clk_cpu", 16 * vcml::MHz),
      m_reset("rst"),
      m_uart0("uart0"),
      m_plic("plic"),
      m_simdev("simdev"),
      m_uart_injector("uart_injector"),
      // Inside your top-level SoC class definition: 
      // Inside your top-level SoC constructor initializer list:
      m_multicore_simdev("multicore_simdev", 2) // Instantiating with 2 cores
{

  tlm_bind(m_bus, m_loader, "insn");
  tlm_bind(m_bus, m_loader, "data");
  tlm_bind(m_bus, m_ram, "in", ram);
  tlm_bind(m_bus, m_bram, "in", bram);
  tlm_bind(m_bus, m_plic, "in", addr_plic);
  tlm_bind(m_bus, m_uart0, "in", addr_uart0);
  tlm_bind(m_bus, m_core2, "insn");
  tlm_bind(m_bus, m_core2, "data");
  tlm_bind(m_bus, m_simdev, "in", addr_simdev);
  // 2. Multicore Simdev an den Bus binden
  tlm_bind(m_bus, m_multicore_simdev, "in", addr_multicore_simdev);

  tlm_bind(m_bus, m_core, "insn");
  tlm_bind(m_bus, m_core, "data");

  clk_bind(m_clock_cpu, "clk", m_core, "clk");
  clk_bind(m_clock_cpu, "clk", m_core2, "clk");
  clk_bind(m_clock_cpu, "clk", m_ram, "clk");
  clk_bind(m_clock_cpu, "clk", m_bram, "clk");
  clk_bind(m_clock_cpu, "clk", m_bus, "clk");
  clk_bind(m_clock_cpu, "clk", m_loader, "clk");
  clk_bind(m_clock_cpu, "clk", m_plic, "clk");
  clk_bind(m_clock_cpu, "clk", m_uart0, "clk");
  clk_bind(m_clock_cpu, "clk", m_simdev, "clk");
  // 3. Takt an das Multicore Simdev binden
  clk_bind(m_clock_cpu, "clk", m_multicore_simdev, "clk");

  gpio_bind(m_reset, "rst", m_core, "rst");
  gpio_bind(m_reset, "rst", m_core2, "rst");
  gpio_bind(m_reset, "rst", m_bus, "rst");
  gpio_bind(m_reset, "rst", m_ram, "rst");
  gpio_bind(m_reset, "rst", m_bram, "rst");
  gpio_bind(m_reset, "rst", m_loader, "rst");
  gpio_bind(m_reset, "rst", m_plic, "rst");
  gpio_bind(m_reset, "rst", m_uart0, "rst");
  gpio_bind(m_reset, "rst", m_simdev, "rst");
  gpio_bind(m_reset, "rst", m_multicore_simdev, "rst");
  // Connect the uart irq to the plic (target socket)
  gpio_bind(m_uart0, "irq", m_plic, "irqs", IRQ_UART0);

  // Connect the core irq to the plic (init socket)
  // gpio_bind(m_core, "irq", m_plic, "irqt"); // is this correct? does gpio bind work with arrays?
  m_plic.irqt[0].bind(m_core.irq[0]);

  m_uart_injector.uart_tx.bind(m_uart0.serial_rx);
  m_uart0.serial_tx.stub();
}

system::~system()
{
  // Wipe the entire folder and its contents
  std::error_code ec;
  std::filesystem::remove_all("./isolated_libs", ec);
  
  if (ec) {
      mwr::log_warn("Failed to clean up isolated_libs: %s", ec.message().c_str());
  } else {
      mwr::log_info("Cleaned up entire isolated_libs directory.");
  }
}

void system::inject_data(sc_core::sc_time period)
{
  sc_core::sc_spawn([this, period]() mutable
                    { 
      wait(period);
      uint8_t data = 15;
      m_uart_injector.send_to_guest(data); 
      vcml::log_info("Data Injected"); });
}

int system::run()
{
  inject_data(sc_core::sc_time(0.05, sc_core::SC_MS));
  double simstart = mwr::timestamp();
  int result = vcml::system::run();
  double realtime = mwr::timestamp() - simstart;
  double duration = sc_core::sc_time_stamp().to_seconds();
  vcml::u64 ninsn = m_core.cycle_count();

  double mips = realtime == 0.0 ? 0.0 : ninsn / realtime / 1e6;
  vcml::log_info("total");
  vcml::log_info("  duration       : %.9fs", duration);
  vcml::log_info("  runtime        : %.4fs", realtime);
  vcml::log_info("  instructions core 0   : %llu", m_core.cycle_count());
  vcml::log_info("  instructions core 1   : %llu", m_core2.cycle_count());
  vcml::log_info("  sim speed      : %.1f MIPS", mips);
  vcml::log_info("  realtime ratio : %.2f / 1s",
                 realtime == 0.0 ? 0.0 : realtime / duration);

  return result;
}