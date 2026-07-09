#ifndef SYSTEM_H
#define SYSTEM_H

#include <vcml.h>
#include "core.h"
#include "vcml/models/riscv/plic.h"
#include "uart_injector.h"

/* Eg where the data/instruction mem separation
   could break the simulator:
   li t0, 0x6f            # Load the instruction bytes into a register
   sd t0, 0x1000(x0)      # Store it to address 0x1000
   jalr x0, 0x1000(x0)    # Jump to address 0x1000
   - The simulator executes sd t0, 0x1000(x0), calls the write callback
   and writes the value 0x6f in the SystemC memory
   - then the simulator fetches the instruction at 0x1000 (jalr) but it
   does it from the ISS internal mem! 
*/

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
  
  

  system(const sc_core::sc_module_name &nm);
  virtual ~system();
  VCML_KIND(sysc_vp::system);
  //virtual const char *version() const override;

  virtual int run() override;

 private:
  PydrofoilCore m_core;

  vcml::generic::bus     m_bus;
  vcml::generic::memory  m_ram;
  vcml::generic::memory  m_bram;

  // A throttle ensures the simulation runs 
  // at a controlled pace, not faster than real time.
  vcml::meta::throttle m_throttle;
  vcml::meta::loader   m_loader;

  vcml::generic::clock m_clock_cpu;
  vcml::generic::reset m_reset;
  
  vcml::serial::uart8250  m_uart0;
  vcml::riscv::plic    m_plic;
  UartInjector         m_uart_injector;
  vcml::riscv::clint   m_clint;
  vcml::sd::card m_sdcard;
  vcml::sd::sdhci m_sdhci;
  vcml::serial::terminal m_term0;

  void inject_data(sc_core::sc_time period);
};

#endif