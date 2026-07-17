/******************************************************************************
 *                                                                            *
 * Copyright 2026 Chiara Ghinami                                              *
 *                                                                            *
 * This software is licensed under the MIT license found in the               *
 * LICENSE file at the root directory of this source tree.                    *
 *                                                                            *
 ******************************************************************************/

#ifndef UART_INJECTOR_HPP
#define UART_INJECTOR_HPP

#include "vcml.h"

namespace injector {

class UartInjector : public vcml::module, public vcml::serial_host {
    private:
    vcml::sc_event sc_ev;
    uint8_t uart_data;

    void uart_transmit();

    public:
    vcml::serial_initiator_socket uart_tx;

    UartInjector(const sc_core::sc_module_name& nm);

    void send_to_guest(uint8_t data);

    virtual ~UartInjector();
};

} // namespace injector

#endif
