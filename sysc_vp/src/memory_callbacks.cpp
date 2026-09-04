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

// Serialises every atomic of every hart against every other one. All harts call
// into this single VP binary, so unlike a lock inside the per-hart .so copies
// this one is actually shared.
std::mutex g_amo_mutex;

struct Reservation {
    bool valid;
    uint64_t addr;
};

// Keyed by hart id, guarded by g_amo_mutex.
std::unordered_map<uint64_t, Reservation> g_reservations;

void invalidate_reservations(uint64_t word_addr)
{
    for(auto& entry : g_reservations) {
        if(entry.second.valid && entry.second.addr == word_addr)
            entry.second.valid = false;
    }
}

void invalidate_reservations_cb(uint64_t word_addr)
{
    std::lock_guard<std::mutex> guard(g_amo_mutex);
    for(auto& entry : g_reservations) {
        if(entry.second.valid && entry.second.addr == word_addr)
            entry.second.valid = false;
    }
    // std::cout << "Invalidated reservations for word address: 0x" << std::hex << word_addr << std::dec << std::endl;
}

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
        std::cout << "[SUCCEEDED] Hart " << core->m_hart_id << " | WRITE to addr: 0x" << std::hex << address << std::dec
                  << " | status: " << (success ? "OK" : "FAILED") << std::endl;
    }

    return success ? 0 : 1;
}

int write_mem_inv(void* cpu, uint64_t address, int size, uint64_t value, void* payload)
{
    invalidate_reservations(address);

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
        std::cout << "[SUCCEEDED] Hart " << core->m_hart_id << " | WRITE to addr: 0x" << std::hex << address << std::dec
                  << " | status: " << (success ? "OK" : "FAILED") << std::endl;
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

namespace {

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

// RAM is handed to us as DMI, so an atomic normally needs no bus transaction at
// all. mem_regions only ever grows and is filled during the first quanta.
uint32_t* dmi_word_ptr(core::PydrofoilCore* core, uint64_t addr)
{
    for(const auto& entry : core->mem_regions) {
        const auto& region = entry.second;
        if(addr >= region.start_addr && addr + 4 <= region.start_addr + region.size)
            return reinterpret_cast<uint32_t*>(region.ptr + (addr - region.start_addr));
    }
    return nullptr;
}

bool amo_load(core::PydrofoilCore* core, uint64_t addr, uint32_t& out)
{
    if(uint32_t* ptr = dmi_word_ptr(core, addr)) {
        out = __atomic_load_n(ptr, __ATOMIC_SEQ_CST);
        return true;
    }

    uint64_t tmp = 0;
    if(read_mem(nullptr, addr, 4, &tmp, core) != 0)
        return false;
    out = static_cast<uint32_t>(tmp);
    return true;
}

bool amo_store(core::PydrofoilCore* core, uint64_t addr, uint32_t value)
{
    if(uint32_t* ptr = dmi_word_ptr(core, addr)) {
        __atomic_store_n(ptr, value, __ATOMIC_SEQ_CST);
        return true;
    }

    return write_mem(nullptr, addr, 4, value, core) == 0;
}

bool amo_compute(uint32_t funct5, uint32_t old_value, uint32_t operand, uint32_t& new_value)
{
    const int32_t s_old = static_cast<int32_t>(old_value);
    const int32_t s_operand = static_cast<int32_t>(operand);

    switch(funct5) {
    case FUNCT5_AMOSWAP:
        new_value = operand;
        return true;
    case FUNCT5_AMOADD:
        new_value = old_value + operand;
        return true;
    case FUNCT5_AMOXOR:
        new_value = old_value ^ operand;
        return true;
    case FUNCT5_AMOOR:
        new_value = old_value | operand;
        return true;
    case FUNCT5_AMOAND:
        new_value = old_value & operand;
        return true;
    case FUNCT5_AMOMIN:
        new_value = s_old <= s_operand ? old_value : operand;
        return true;
    case FUNCT5_AMOMAX:
        new_value = s_old >= s_operand ? old_value : operand;
        return true;
    case FUNCT5_AMOMINU:
        new_value = old_value <= operand ? old_value : operand;
        return true;
    case FUNCT5_AMOMAXU:
        new_value = old_value >= operand ? old_value : operand;
        return true;
    default:
        return false;
    }
}

} // namespace

int atomic_mem(void* cpu, uint32_t insn, uint64_t address, uint64_t src, uint64_t* result, void* payload)
{
    auto core = reinterpret_cast<core::PydrofoilCore*>(payload);

    if((insn & 0x7f) != AMO_OPCODE)
        return 1;
    if(((insn >> 12) & 0x7) != AMO_WIDTH_W)
        return 1;

    const uint32_t funct5 = (insn >> 27) & 0x1f;
    const uint64_t addr = address & ~uint64_t(3);
    const uint32_t operand = static_cast<uint32_t>(src);

    std::lock_guard<std::mutex> guard(g_amo_mutex);

    if(funct5 == FUNCT5_LR) {
        uint32_t value = 0;
        if(!amo_load(core, addr, value))
            return 1;
        g_reservations[core->m_hart_id] = Reservation{true, addr};
        *result = value;
        return 0;
    }

    if(funct5 == FUNCT5_SC) {
        auto it = g_reservations.find(core->m_hart_id);
        const bool held = it != g_reservations.end() && it->second.valid && it->second.addr == addr;

        if(held) {
            if(!amo_store(core, addr, operand))
                return 1;
            invalidate_reservations(addr);
            *result = 0;
        } else {
            *result = 1;
        }

        // A store-conditional always ends the issuing hart's reservation, no
        // matter whether it succeeded.
        g_reservations[core->m_hart_id] = Reservation{false, 0};
        return 0;
    }

    uint32_t old_value = 0;
    if(!amo_load(core, addr, old_value))
        return 1;

    uint32_t new_value = 0;
    if(!amo_compute(funct5, old_value, operand, new_value))
        return 1;

    if(!amo_store(core, addr, new_value))
        return 1;

    invalidate_reservations(addr);
    *result = old_value;
    return 0;
}
