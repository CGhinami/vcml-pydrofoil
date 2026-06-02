#ifndef VCML_META_MULTICORE_SIMDEV_H
#define VCML_META_MULTICORE_SIMDEV_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"
#include "vcml/protocols/tlm.h"

namespace vcml {
namespace meta {

class multicore_simdev : public peripheral
{
private:
    u64 m_done_mask;
    unsigned int m_num_cores; // <-- Replaced property with standard variable

    // Callbacks
    void write_core_done(u32 val);
    void write_sout(u32 val);

    // Disabled constructors
    multicore_simdev();
    multicore_simdev(const multicore_simdev&);

public:
    // Registers
    reg<u32> core_done; // Offset: 0x00
    reg<u32> sout;      // Offset: 0x08

    // TLM Socket for the bus
    tlm_target_socket in;

    // <-- Updated constructor signature
    multicore_simdev(const sc_module_name& name, unsigned int num_cores); 
    virtual ~multicore_simdev();
    
    VCML_KIND(multicore_simdev);

    virtual void reset() override;
};

} // namespace meta
} // namespace vcml

#endif