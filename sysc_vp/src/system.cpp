/******************************************************************************
 *                                                                            *
 * Copyright 2026 Chiara Ghinami                                              *
 *                                                                            *
 * This software is licensed under the MIT license found in the               *
 * LICENSE file at the root directory of this source tree.                    *
 *                                                                            *
 ******************************************************************************/

#include "system.h"
#include <atomic>
#include <cstdint>

extern std::atomic<uint64_t> g_slowpath_write_count;
extern std::atomic<uint64_t> g_slowpath_read_count;

namespace virtual_platform {

system::system(const sc_core::sc_module_name& nm):
    vcml::system(nm),
    ram("ram", {SRAM_LO, SRAM_HI}),
    bram("bram", {BOOT_LO, BOOT_HI}),
    addr_uart0("addr_uart0", {UART0_LO, UART0_HI}),
    addr_plic("addr_plic", {PLIC_LO, PLIC_HI}),
    addr_clint("addr_clint", {CLINT_LO, CLINT_HI}),
    addr_simdev("addr_simdev", {SIMDEV_LO, SIMDEV_HI}),
    addr_multicore_simdev("addr_multicore_simdev", {MULTICORE_SIMDEV_LO, MULTICORE_SIMDEV_HI}),
    addr_uart8250("addr_uart8250", {UART8250_LO, UART8250_HI}),
    addr_hwrng("addr_hwrng", {HWRNG_LO, HWRNG_HI}),
    addr_virtio0("addr_virtio0", {VIRTIO0_LO, VIRTIO0_HI}),
    irq_uart0("irq_uart0", IRQ_UART0),
    irq_uart8250("irq_uart8250", IRQ_UART8250),
    irq_virtio0("irq_virtio0", IRQ_VIRTIO0),
    m_core("core"),
    m_bus("bus"),
    m_ram("sram", ram.get().length()),
    m_bram("bram", bram.get().length()),
    m_throttle("throttle"),
    m_loader("loader"),
    m_clock_cpu("clk_cpu", 16 * vcml::MHz),
    m_clock_rtc("clk_clint", 10 * vcml::MHz),
    m_reset("rst"),
    m_uart0("uart0"),
    m_plic("plic"),
    m_clint("clint"),
    m_term("term"),
    m_simdev("simdev"),
    m_multicore_simdev("multicore_simdev", 1), // single core for now
    m_uart8250("uart8250"),
    m_term8250("term8250"),
    m_hwrng("hwrng"),
    m_virtio0("virtio0"),
    m_virtio_blk("virtio_blk")
{
    tlm_bind(m_bus, m_loader, "insn");
    tlm_bind(m_bus, m_loader, "data");
    tlm_bind(m_bus, m_ram, "in", ram);
    tlm_bind(m_bus, m_bram, "in", bram);
    tlm_bind(m_bus, m_plic, "in", addr_plic);
    tlm_bind(m_bus, m_clint, "in", addr_clint);
    tlm_bind(m_bus, m_uart0, "in", addr_uart0);
    tlm_bind(m_bus, m_simdev, "in", addr_simdev);
    tlm_bind(m_bus, m_multicore_simdev, "in", addr_multicore_simdev);
    tlm_bind(m_bus, m_uart8250, "in", addr_uart8250);
    tlm_bind(m_bus, m_hwrng, "in", addr_hwrng);
    tlm_bind(m_bus, m_virtio0, "in", addr_virtio0);
    // virtqueues live in guest RAM, so the controller needs to be a bus
    // master as well as a target.
    tlm_bind(m_bus, m_virtio0, "out");

    tlm_bind(m_bus, m_core, "insn");
    tlm_bind(m_bus, m_core, "data");

    clk_bind(m_clock_cpu, "clk", m_core, "clk");
    clk_bind(m_clock_cpu, "clk", m_ram, "clk");
    clk_bind(m_clock_cpu, "clk", m_bram, "clk");
    clk_bind(m_clock_cpu, "clk", m_bus, "clk");
    clk_bind(m_clock_cpu, "clk", m_loader, "clk");
    clk_bind(m_clock_cpu, "clk", m_plic, "clk");
    clk_bind(m_clock_rtc, "clk", m_clint, "clk");   // This is a temporary patch
    clk_bind(m_clock_cpu, "clk", m_uart0, "clk");
    clk_bind(m_clock_cpu, "clk", m_simdev, "clk");
    clk_bind(m_clock_cpu, "clk", m_multicore_simdev, "clk");
    clk_bind(m_clock_cpu, "clk", m_uart8250, "clk");
    clk_bind(m_clock_cpu, "clk", m_hwrng, "clk");
    clk_bind(m_clock_cpu, "clk", m_virtio0, "clk");

    gpio_bind(m_reset, "rst", m_core, "rst");
    gpio_bind(m_reset, "rst", m_bus, "rst");
    gpio_bind(m_reset, "rst", m_ram, "rst");
    gpio_bind(m_reset, "rst", m_bram, "rst");
    gpio_bind(m_reset, "rst", m_loader, "rst");
    gpio_bind(m_reset, "rst", m_plic, "rst");
    gpio_bind(m_reset, "rst", m_clint, "rst");
    gpio_bind(m_reset, "rst", m_uart0, "rst");
    gpio_bind(m_reset, "rst", m_simdev, "rst");
    gpio_bind(m_reset, "rst", m_multicore_simdev, "rst");
    gpio_bind(m_reset, "rst", m_uart8250, "rst");
    gpio_bind(m_reset, "rst", m_hwrng, "rst");
    gpio_bind(m_reset, "rst", m_virtio0, "rst");

    // Connect the uart irq to the plic (target socket)
    gpio_bind(m_uart0, "irq", m_plic, "irqs", IRQ_UART0);
    gpio_bind(m_uart8250, "irq", m_plic, "irqs", IRQ_UART8250);
    gpio_bind(m_virtio0, "irq", m_plic, "irqs", IRQ_VIRTIO0);

    // Connect the core irq to the plic (init socket).
    //
    // One PLIC context per privilege level, in the order the guest device
    // tree lists them under interrupts-extended: context 0 is hart 0's
    // M-mode external interrupt, context 1 its S-mode one. Firmware running
    // in M-mode claims from the first, the OS from the second; a kernel
    // booted under SBI only ever programs context 1, so without this second
    // binding its external interrupts are enabled in the PLIC and never
    // delivered.
    m_plic.irqt[0].bind(m_core.irq[core::MEIP]);
    m_plic.irqt[1].bind(m_core.irq[core::SEIP]);

    // Only one core for now
    gpio_bind(m_clint, "irq_timer", 0, m_core, "irq", core::MTIP);
    gpio_bind(m_clint, "irq_sw",    0, m_core, "irq", core::MSIP);

    virtio_bind(m_virtio0, "virtio_out", m_virtio_blk, "virtio_in");

    serial_bind(m_term, "serial_tx", m_uart0, "serial_rx");
    serial_bind(m_term, "serial_rx", m_uart0, "serial_tx");

    serial_bind(m_term8250, "serial_tx", m_uart8250, "serial_rx");
    serial_bind(m_term8250, "serial_rx", m_uart8250, "serial_tx");

    const auto cpu_hz = m_clock_cpu.hz.get();
    const auto rtc_hz = m_clock_rtc.hz.get();
    if(rtc_hz == 0 || cpu_hz % rtc_hz != 0)
        vcml::log_error("clk_cpu (%llu Hz) must be an integer multiple of clk_clint (%llu Hz)",
                    (unsigned long long)cpu_hz, (unsigned long long)rtc_hz);
    
    m_core.set_insns_tick(cpu_hz / rtc_hz);
}

system::~system()
{
    // nothing to do
}

int system::run()
{
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
    vcml::log_info("  realtime ratio : %.2f / 1s", realtime == 0.0 ? 0.0 : realtime / duration);
    // wall-clock time since the guest last read multicore_simdev.hclk (benchmark start)
    if(m_multicore_simdev.last_queried_time > 0.0)
        vcml::log_info("  benchmark time : %.4fs", mwr::timestamp() - m_multicore_simdev.last_queried_time);
    vcml::log_info("  slowpath reads : %llu", (unsigned long long) g_slowpath_read_count.load());
    vcml::log_info("  slowpath writes: %llu", (unsigned long long) g_slowpath_write_count.load());

    return result;
}

} // namespace virtual_platform
