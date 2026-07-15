/******************************************************************************
 *                                                                            *
 * Copyright 2026 Chiara Ghinami                                              *
 *                                                                            *
 * This software is licensed under the MIT license found in the               *
 * LICENSE file at the root directory of this source tree.                    *
 *                                                                            *
 ******************************************************************************/

#include "uart_injector.h"

namespace injector {

void UartInjector::uart_transmit()
{
    while(true) {
        wait(sc_ev);
        uart_tx.send(uart_data);
    }
}

void UartInjector::send_to_guest(uint8_t data)
{
    uart_data = data;
    vcml::on_next_update([&]() -> void { sc_ev.notify(sc_core::SC_ZERO_TIME); });
}

UartInjector::UartInjector(const sc_core::sc_module_name& nm): module(nm), sc_ev("rxev"), uart_tx("uart_tx")
{
    SC_HAS_PROCESS(UartInjector);
    SC_THREAD(uart_transmit);
}

UartInjector::~UartInjector() {}

} // namespace injector
