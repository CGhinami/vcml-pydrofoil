# b core.cpp:handle_breakpoint_hit
# b core.cpp:simulate
# b core.cpp:interrupt

b uart_injector.cpp:6
b plic.cpp:read_claim
b plic.cpp:write_complete
# b nrf51.cpp:write_txd
# b nrf51.cpp:write_rxd

# b gdbserver.cpp:notify_breakpoint_hit
# b gdbserver.cpp:notify_step_complete
# b gdbserver.cpp:handle_step
# b gdbserver.cpp:handle_continue
# b gdbserver.cpp:handle_detach
# b gdbserver.cpp:handle_kill
# b gdbserver.cpp:handle_rcmd
# b gdbserver.cpp:handle_breakpoint_set
# b gdbserver.cpp:handle_breakpoint_delete
# b gdbserver.cpp:handle_vcont
# run










