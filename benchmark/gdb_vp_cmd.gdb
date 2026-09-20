# b core.cpp:handle_breakpoint_hit
# b core.cpp:simulate
# b core.cpp:interrupt
# b PydrofoilCore::interrupt
# b core.cpp:346
# b atomic_mem
b vcml::riscv::clint::write_mtimecmp
b mip_bit_for_irq
run










