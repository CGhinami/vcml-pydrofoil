#include "system.h"

system::system(const sc_core::sc_module_name &nm)
    : vcml::system(nm), 
    ram("ram", {SRAM_LO, SRAM_HI}),
    bram("bram", {BOOT_LO, BOOT_HI}),
    addr_uart0("addr_uart0", {UART0_LO, UART0_HI}),
    addr_plic("addr_plic", {PLIC_LO, PLIC_HI}),
    addr_clint("addr_clint", {CLINT_LO, CLINT_HI}),
    addr_sdhci("addr_sdhci", {SDHCI_LO, SDHCI_HI}),
    irq_uart0("irq_uart0", IRQ_UART0),
    irq_sdhci("irq_sdhci", IRQ_SDHCI),
    m_core("core"),
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
    m_sdcard("sdcard"),
    m_sdhci("sdhci"),
    m_term0("term0"),
    m_uart_injector("uart_injector") {

    tlm_bind(m_bus, m_loader, "insn");
    tlm_bind(m_bus, m_loader, "data");
    tlm_bind(m_bus, m_ram, "in", ram);
    tlm_bind(m_bus, m_bram, "in", bram);
    tlm_bind(m_bus, m_plic, "in", addr_plic);
    tlm_bind(m_bus, m_uart0, "in", addr_uart0);
    tlm_bind(m_bus, m_sdhci, "in", addr_sdhci);
    tlm_bind(m_bus, m_clint, "in", addr_clint);

    
    tlm_bind(m_bus, m_sdhci, "out");

    tlm_bind(m_bus, m_core, "insn");
    tlm_bind(m_bus, m_core, "data");

    clk_bind(m_clock_cpu, "clk", m_core, "clk");
    clk_bind(m_clock_cpu, "clk", m_ram, "clk");
    clk_bind(m_clock_cpu, "clk", m_bram, "clk");
    clk_bind(m_clock_cpu, "clk", m_bus, "clk");
    clk_bind(m_clock_cpu, "clk", m_loader, "clk");
    clk_bind(m_clock_cpu, "clk", m_plic, "clk");
    clk_bind(m_clock_cpu, "clk", m_uart0, "clk");
    clk_bind(m_clock_cpu, "clk", m_clint, "clk");
    clk_bind(m_clock_cpu, "clk", m_sdhci, "clk");
    clk_bind(m_clock_cpu, "clk", m_sdcard, "clk");

    gpio_bind(m_reset, "rst", m_core, "rst");
    gpio_bind(m_reset, "rst", m_bus, "rst");
    gpio_bind(m_reset, "rst", m_ram, "rst");
    gpio_bind(m_reset, "rst", m_bram, "rst");
    gpio_bind(m_reset, "rst", m_loader, "rst");
    gpio_bind(m_reset, "rst", m_plic, "rst");
    gpio_bind(m_reset, "rst", m_uart0, "rst");
    gpio_bind(m_reset, "rst", m_clint, "rst");
    gpio_bind(m_reset, "rst", m_sdhci, "rst");
    gpio_bind(m_reset, "rst", m_sdcard, "rst");

    sd_bind(m_sdhci, "sd_out", m_sdcard, "sd_in");
    // Connect the uart irq to the plic (target socket)
    gpio_bind(m_uart0, "irq", m_plic, "irqs", IRQ_UART0);
    gpio_bind(m_sdhci, "irq", m_plic, "irqs", IRQ_SDHCI);

    serial_bind(m_term0, "serial_tx", m_uart0, "serial_rx");
    serial_bind(m_term0, "serial_rx", m_uart0, "serial_tx");

    // Connect the core irq to the plic (init socket)
    //gpio_bind(m_core, "irq", m_plic, "irqt"); // is this correct? does gpio bind work with arrays?
    // Context 0 (M-Mode) an den Machine-External-Interrupt-Pin (11) klemmen
    m_plic.irqt[0].bind(m_core.irq[MEIP]); 

    // Context 1 (S-Mode) an den Supervisor-External-Interrupt-Pin (9) klemmen
    // (Das zwingt VCML dazu, die Register für Context 1 bei 0x2080 anzulegen!)
    m_plic.irqt[1].bind(m_core.irq[SEIP]);
    m_clint.irq_sw[0].bind(m_core.irq[MSIP]); 
    m_clint.irq_timer[0].bind(m_core.irq[MTIP]);

    // m_uart_injector.uart_tx.bind(m_uart0.serial_rx);
    m_uart_injector.uart_tx.stub();
    // m_uart0.serial_tx.stub();
}

system::~system() {
  // nothing to do
}


void system::inject_data(sc_core::sc_time period)
{
    sc_core::sc_spawn( [this, period]() mutable 
    { 
      wait(period);
      uint8_t data = 15;
      m_uart_injector.send_to_guest(data); 
      vcml::log_info("Data Injected");
  });
}


int system::run() {
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
    vcml::log_info("  instructions   : %llu", ninsn);
    vcml::log_info("  sim speed      : %.1f MIPS", mips);
    vcml::log_info("  realtime ratio : %.2f / 1s",
                   realtime == 0.0 ? 0.0 : realtime / duration);

    return result;
}