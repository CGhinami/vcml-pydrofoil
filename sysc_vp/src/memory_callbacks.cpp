/******************************************************************************
 *                                                                            *
 * Copyright 2026 Chiara Ghinami                                              *
 *                                                                            *
 * This software is licensed under the MIT license found in the               *
 * LICENSE file at the root directory of this source tree.                    *
 *                                                                            *
 ******************************************************************************/

#include "memory_callbacks.h"
#include "core.h"
#include <cstring> // for memset
#include <iostream>
#include <mutex>
#include <unordered_map>
 
 // C++ member functions cannot be used as callbacks, we need to define C-style functions
 // (not member of the class), but they still need to get access to the class fields
 // so we misuse the payload pointer to pass this as argument
 int write_mem(void* cpu, uint64_t address, int size, uint64_t value, void* payload)
 {
     auto core = reinterpret_cast<core::PydrofoilCore*>(payload);
 
     // std::cout << "[REQUEST] Hart " << core->m_hart_id << " | WRITE to addr: 0x" << std::hex << address
     //           << " | size: " << std::dec << size << " bytes"
     //           << " | value: 0x" << std::hex << value << std::dec << std::endl;
 
     core::PydrofoilCore::MemAccess memtask;
 
     memtask.type = core::PydrofoilCore::MemTask::Write;
     memtask.addr = address;
     memtask.size = size;
     memtask.value = value;
 
     std::future<bool> res = memtask.result.get_future();
 
     {
         std::lock_guard lock(core->memtask_mutex);
         core->memtask_queue.push(std::move(memtask));
     }
     core->memtask_cv.notify_one();
 
     bool success = res.get();
 
    if(!success) {
        std::cout << "[SUCCEEDED] Hart " << core->m_hart_id << " | WRITE to addr: 0x" << std::hex << address
                  << std::dec << " | status: " << (success ? "OK" : "FAILED") << std::endl;
    }

    return success ? 0 : 1;
}

// The debug leads to a debug transaction avoid timig annotation --> no wait --> we dont have to be in a sc_thread
int read_mem(void* cpu, uint64_t address, int size, void* destination, void* payload)
 {
     auto core = reinterpret_cast<core::PydrofoilCore*>(payload);
 
     // std::cout << "[REQUEST] Hart " << core->m_hart_id << " | READ from addr: 0x" << std::hex << address
     //           << " | size: " << std::dec << size << " bytes" << std::endl;
 
     core::PydrofoilCore::MemAccess memtask;
 
     memtask.type = core::PydrofoilCore::MemTask::Read;
     memtask.addr = address;
     memtask.size = size;
     memtask.dest = destination;
 
     std::future<bool> res = memtask.result.get_future();
 
     {
         std::lock_guard lock(core->memtask_mutex);
         core->memtask_queue.push(std::move(memtask));
     }
     core->memtask_cv.notify_one();
 
     bool success = res.get();
 
    if(!success) {
        std::cout << "[SUCCEEDED] Hart " << core->m_hart_id << " | READ from addr: 0x" << std::hex << address
                  << std::dec << " | status: " << (success ? "OK" : "FAILED") << std::endl;
    }

    return success ? 0 : 1;
}

enum : uint32_t {
    AMO_OPCODE = 0x2f,
    AMO_WIDTH_W = 2, // funct3 encoding for the 32 bit variants

    FUNCT5_AMOADD = 0x00,
    FUNCT5_AMOSWAP = 0x01,
    FUNCT5_LR = 0x02,
    FUNCT5_SC = 0x03,
    FUNCT5_AMOXOR = 0x04,
    FUNCT5_AMOOR = 0x08,
    FUNCT5_AMOAND = 0x0c,
    FUNCT5_AMOMIN = 0x10,
    FUNCT5_AMOMAX = 0x14,
    FUNCT5_AMOMINU = 0x18,
    FUNCT5_AMOMAXU = 0x1c
};

int atomic_mem(void* cpu, uint32_t insn, uint64_t address, uint64_t src, uint64_t* result, void* payload)
{
    auto core = reinterpret_cast<core::PydrofoilCore*>(payload);

    if((insn & 0x7f) != AMO_OPCODE || ((insn >> 12) & 0x7) != AMO_WIDTH_W)
        return 1;

    const uint32_t funct5 = (insn >> 27) & 0x1f;
    const uint64_t addr = address & ~uint64_t(3);
    const uint32_t operand = static_cast<uint32_t>(src);

    core::PydrofoilCore::MemAccess memtask;
    memtask.addr = addr;
    memtask.size = 4;
    memtask.funct5 = funct5;

    // Je nach AMO-Typ den MemTask vorbereiten
    if (funct5 == FUNCT5_LR) {
        memtask.type = core::PydrofoilCore::MemTask::LR;
        memtask.dest = result; // Ziel für den gelesenen Wert
    } 
    else if (funct5 == FUNCT5_SC) {
        memtask.type = core::PydrofoilCore::MemTask::SC;
        memtask.value = operand; // Was geschrieben werden soll
        memtask.dest = result;   // Ziel für den Error-Code (0=OK, 1=Fail)
    } 
    else {
        memtask.type = core::PydrofoilCore::MemTask::AMO;
        memtask.value = operand;
        memtask.dest = result;   // Ziel für den alten Wert
    }

    std::future<bool> res = memtask.result.get_future();

    {
        // Mutex für das Einreihen in die Queue (verhindert Race-Conditions mit dem SystemC Thread)
        std::lock_guard lock(core->memtask_mutex);
        core->memtask_queue.push(std::move(memtask));
    }
    core->memtask_cv.notify_one();

    bool success = res.get(); // Wartet auf SystemC-Kernel

    return success ? 0 : 1;
}