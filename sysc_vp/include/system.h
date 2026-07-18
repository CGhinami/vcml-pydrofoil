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
#include "uart_injector.h"

namespace virtual_platform {

enum : mwr::u64 {
  SRAM_SZ = 256 * mwr::MiB,
  SRAM_LO = 0x80000000,
  SRAM_HI = SRAM_LO + SRAM_SZ - 1,

    BOOT_SZ = 4 * mwr::KiB,
    BOOT_LO = 0x00001000,
    BOOT_HI = BOOT_LO + BOOT_SZ - 1,

    UART0_LO = 0x10009000,
    UART0_HI = UART0_LO + 0x1000 - 1,

  PLIC_LO = 0x1000a000,
  PLIC_HI = PLIC_LO + 0x224FFF -1,

  CLINT_LO = 0x15000000,
  CLINT_HI = CLINT_LO + 0x10000 - 1, // 64KB large

  SDHCI_LO = 0x15020000,
  SDHCI_HI = SDHCI_LO + 0x1000 - 1
};


enum : mwr::u64 {
  IRQ_UART0 = 5,
  IRQ_SDHCI = 6
};

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
  vcml::property<range> addr_sdhci;
  vcml::property<int>   irq_uart0;
  vcml::property<int>   irq_sdhci;



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
  vcml::generic::reset m_reset;
  
  vcml::serial::uart8250  m_uart0;
  vcml::riscv::plic    m_plic;
  injector::UartInjector  m_uart_injector;
  vcml::riscv::clint   m_clint;
  vcml::sd::card m_sdcard;
  vcml::sd::sdhci m_sdhci;
  vcml::serial::terminal m_term0;
};

} // namespace virtual_platform

#endif
