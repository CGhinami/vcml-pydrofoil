#include "memory_callbacks.h"
#include <cstring>   // for memset


// C++ member functions cannot be used as callbacks, we need to define C-style functions
// (not member of the class), but they still need to get access to the class fields
// so we misuse the payload pointer to pass this as argument
int write_mem(void* cpu, uint64_t address, int size, uint64_t value, void* payload) 
{
    auto core = reinterpret_cast<PydrofoilCore*>(payload);

    PydrofoilCore::MemAccess memtask;

    memtask.type = PydrofoilCore::MemTask::Write;
    memtask.addr = address;
    memtask.size = size;
    memtask.value = value;

    std::future<bool> res = memtask.result.get_future();

    {
        std::lock_guard lock(core->memtask_mutex);
        core->memtask_queue.push(std::move(memtask));
    }
    core->memtask_cv.notify_one();

    return res.get()? 0:1;
}


// The debug leads to a debug transaction avoid timig annotation --> no wait --> we dont have to be in a sc_thread
int read_mem(void* cpu, uint64_t address, int size, uint64_t* destination, void* payload) 
{
    auto core = reinterpret_cast<PydrofoilCore*>(payload);

    PydrofoilCore::MemAccess memtask;

    memtask.type = PydrofoilCore::MemTask::Read;
    memtask.addr = address; 
    memtask.size = size; 
    memtask.dest = destination;

    std::future<bool> res = memtask.result.get_future();

    {
        std::lock_guard lock(core->memtask_mutex);
        core->memtask_queue.push(std::move(memtask));
    }
    core->memtask_cv.notify_one();

    return res.get()? 0:1;
}