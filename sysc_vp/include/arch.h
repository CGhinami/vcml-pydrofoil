/******************************************************************************
 *                                                                            *
 * Copyright 2026 Chiara Ghinami                                              *
 *                                                                            *
 * This software is licensed under the MIT license found in the               *
 * LICENSE file at the root directory of this source tree.                    *
 *                                                                            *
 ******************************************************************************/

#ifndef ARCH_H
#define ARCH_H

#include <cstring>

namespace architecture {

struct Reg {
    const char* gdb_name;
    // const int    offset;
    // const int    width_bytes;
    const char* x_name;
};

class Model {
    public:
    Model(const char* name, int bits, const Reg* regs, int nregs):
        name(name), bits(bits), registers(regs), nregs(nregs) {};
    Model() = delete;

    bool has_aarch32() const { return bits >= 32; }
    bool has_aarch64() const { return bits >= 64; }

    int word_size() const { return bits / 8; }
    unsigned int reg_number() const { return nregs; }
    const Reg* get_regs_ptr() const { return registers; }

    int find_reg_idx(const char* name)
    {
        for(int i = 0; i < nregs; ++i) {
            if(strcmp(registers[i].gdb_name, name) == 0)
                return i; // Return the index when a match is found
        }
        return -1; // Return -1 if name is not found
    }

    private:
    const char* name;
    int bits;

    const Reg* registers;
    unsigned int nregs;
};

} // namespace architecture

#endif
