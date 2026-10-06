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
#include <atomic>
#include <cstdio>

// TEMP DEBUG INSTRUMENTATION -- counts slow-path (non-DMI) memory callbacks.
std::atomic<uint64_t> g_slowpath_write_count{0};
std::atomic<uint64_t> g_slowpath_read_count{0};

// C++ member functions cannot be used as callbacks, we need to define C-style functions
// (not member of the class), but they still need to get access to the class fields
// so we misuse the payload pointer to pass this as argument
int write_mem(void* cpu, uint64_t address, int size, uint64_t value, void* payload)
{
    g_slowpath_write_count.fetch_add(1, std::memory_order_relaxed);
    auto core = reinterpret_cast<core::PydrofoilCore*>(payload);

    core::PydrofoilCore::MemAccess memtask;

    memtask.type = core::PydrofoilCore::MemTask::Write;
    memtask.addr = address;
    memtask.size = size;
    memtask.value = value;

    // Nothing else for the worker thread to do while this is in flight, so
    // the plain blocking submit() (post + spin-wait) is fine here -- the
    // multiplexed post()/is_done() split is only needed on the other side,
    // in PydrofoilCore::simulate(), which has to watch two mailboxes at once.
    core->memtask_mailbox.submit(memtask);

    return memtask.success ? 0 : 1;
}

// The debug leads to a debug transaction avoid timig annotation --> no wait --> we dont have to be in a sc_thread
int read_mem(void* cpu, uint64_t address, int size, void* destination, void* payload)
{
    g_slowpath_read_count.fetch_add(1, std::memory_order_relaxed);
    auto core = reinterpret_cast<core::PydrofoilCore*>(payload);

    core::PydrofoilCore::MemAccess memtask;

    memtask.type = core::PydrofoilCore::MemTask::Read;
    memtask.addr = address;
    memtask.size = size;
    memtask.dest = destination;

    core->memtask_mailbox.submit(memtask);

    return memtask.success ? 0 : 1;
}
