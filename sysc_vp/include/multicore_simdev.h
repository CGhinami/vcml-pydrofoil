#ifndef VCML_META_MULTICORE_SIMDEV_H
#define VCML_META_MULTICORE_SIMDEV_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"
#include "vcml/protocols/tlm.h"
#include <fstream> // <-- NEU: Für Datei-Output

namespace vcml {
namespace meta {

class multicore_simdev : public peripheral
{
private:
    u64 m_done_mask;
    unsigned int m_num_cores; 

    // <-- NEU: Dateistreams für Core 0 und Core 1
    std::ofstream m_out_core0;
    std::ofstream m_out_core1;

    // Callbacks
    void write_core_done(u32 val);
    void write_sout0(u32 val); // <-- NEU: Callback für Core 0
    void write_sout1(u32 val); // <-- NEU: Callback für Core 1

    // Disabled constructors
    multicore_simdev();
    multicore_simdev(const multicore_simdev&);

public:
    // Registers
    reg<u32> core_done; // Offset: 0x00
    reg<u32> sout0;     // Offset: 0x08 <-- NEU: Register Core 0
    reg<u32> sout1;     // Offset: 0x0C <-- NEU: Register Core 1

    // TLM Socket for the bus
    tlm_target_socket in;

    multicore_simdev(const sc_module_name& name, unsigned int num_cores); 
    virtual ~multicore_simdev();
    
    VCML_KIND(multicore_simdev);

    virtual void reset() override;
};

} // namespace meta
} // namespace vcml

#endif