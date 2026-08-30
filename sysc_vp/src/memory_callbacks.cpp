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
        std::cout << "[SUCCEEDED] Hart " << core->m_hart_id << " | READ from addr: 0x" << std::hex << address
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
