/******************************************************************************
 *                                                                            *
 * Copyright 2026 Chiara Ghinami                                              *
 *                                                                            *
 * This software is licensed under the MIT license found in the               *
 * LICENSE file at the root directory of this source tree.                    *
 *                                                                            *
 ******************************************************************************/

#ifndef SYSTEM_H
#define SYSTEM_H

#include <vcml.h>
#include "core.h"
#include "vcml/models/riscv/plic.h"
#include "vcml/models/serial/uart.h"
#include "vcml/models/generic/hwrng.h"
#include "vcml/models/virtio/mmio.h"
#include "vcml/models/virtio/blk.h"
#include "uart_injector.h"

namespace virtual_platform {

enum : mwr::u64 {
    SRAM_SZ = 256 * mwr::KiB,
    SRAM_LO = 0x80000000,
    SRAM_HI = SRAM_LO + SRAM_SZ - 1,

    BOOT_SZ = 4 * mwr::KiB,
    BOOT_LO = 0x00001000,
    BOOT_HI = BOOT_LO + BOOT_SZ - 1,

    UART0_LO = 0x10009000,
    UART0_HI = UART0_LO + 0x1000 - 1,

    PLIC_LO = 0x1000a000,
    PLIC_HI = PLIC_LO + 0x224FFF - 1,

    CLINT_LO = 0x02000000,
    CLINT_HI = CLINT_LO + 0xC000 - 1,

    SIMDEV_LO = 0x10008000,
    SIMDEV_HI = SIMDEV_LO + 0x1000 - 1,

    // The peripherals below exist for OS images that expect drivers the
    // kernel actually ships: an ns16550a console, a virtio-mmio root disk
    // and an RNG. They sit alongside the nRF51 UART rather than replacing
    // it, because benchmark/interrupt_demo drives that model's register
    // layout directly at UART0_LO.
    UART8250_LO = 0x10000000,
    UART8250_HI = UART8250_LO + 0x1000 - 1,

    HWRNG_LO = 0x10001000,
    HWRNG_HI = HWRNG_LO + 0x1000 - 1,

    VIRTIO0_LO = 0x14000000,
    VIRTIO0_HI = VIRTIO0_LO + 0x1000 - 1
};

// PLIC source numbers. Source 0 is reserved by the PLIC specification and
// vcml rejects it outright.
enum : mwr::u64 { IRQ_UART0 = 5, IRQ_UART8250 = 1, IRQ_VIRTIO0 = 12 };

class system : public vcml::system {
    public:
    using u16 = vcml::u16;
    using u32 = vcml::u32;
    using u64 = vcml::u64;
    using range = vcml::range;

    vcml::property<range> ram;
    vcml::property<range> bram;
    vcml::property<range> addr_uart0;
    vcml::property<range> addr_plic;
    vcml::property<range> addr_clint;
    vcml::property<vcml::range> addr_simdev;
    vcml::property<range> addr_uart8250;
    vcml::property<range> addr_hwrng;
    vcml::property<range> addr_virtio0;
    vcml::property<int> irq_uart0;
    vcml::property<int> irq_uart8250;
    vcml::property<int> irq_virtio0;

    system(const sc_core::sc_module_name& nm);
    virtual ~system();
    VCML_KIND(sysc_vp::system);
    // virtual const char *version() const override;

    virtual int run() override;

    private:
    core::PydrofoilCore m_core;

    vcml::generic::bus m_bus;
    vcml::generic::memory m_ram;
    vcml::generic::memory m_bram;

    // A throttle ensures the simulation runs
    // at a controlled pace, not faster than real time.
    vcml::meta::throttle m_throttle;
    vcml::meta::loader m_loader;

    vcml::generic::clock m_clock_cpu;
    vcml::generic::clock m_clock_rtc;
    vcml::generic::reset m_reset;

    vcml::serial::nrf51 m_uart0;
    vcml::riscv::plic m_plic;
    vcml::riscv::clint m_clint;

    vcml::serial::terminal m_term;
    vcml::meta::simdev m_simdev;

    // ns16550a-compatible console: byte-wide registers at offsets 0..7, so
    // the "ns16550a" device-tree defaults (reg-shift 0, reg-io-width 1) fit
    // without overrides.
    vcml::serial::uart8250 m_uart8250;
    vcml::serial::terminal m_term8250;

    vcml::generic::hwrng m_hwrng;

    vcml::virtio::mmio m_virtio0;
    vcml::virtio::blk m_virtio_blk;
};

} // namespace virtual_platform

#endif
