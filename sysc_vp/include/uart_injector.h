#ifndef UART_INJECTOR_HPP
#define UART_INJECTOR_HPP

#include "vcml/core/component.h"
#include "vcml/protocols/serial.h"

class UartInjector : public vcml::module, public vcml::serial_host{
    private:
        vcml::sc_event                               sc_ev;
        uint8_t                                      uart_data;

        void uart_transmit();

    public:
        vcml::serial_initiator_socket                uart_tx;
        
        UartInjector(const sc_core::sc_module_name &nm);

        void send_to_guest(uint8_t data);

        ~UartInjector();
};

#endif