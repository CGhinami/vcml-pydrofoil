#include "memory_callbacks.h"
#include <cstring>   // for memset


// C++ member functions cannot be used as callbacks, we need to define C-style functions
// (not member of the class), but they still need to get access to the class fields
// so we misuse the payload pointer to pass this as argument
int write_mem(void* cpu, uint64_t address, int size, uint64_t value, void* payload) 
{
    // If we're out of bounds (as it happens) just return
    // Should be changed, NO access when callbacks are being set!!!
    if(address < 0x80000000 && address > 0x1fff){
        std::cout << "peripheral access" << std::endl;
        return 0;
    }
    if(address > 0x8fffffff || address < 0x1000){
        std::cout << "out of bound" << std::endl;
        return 0;
    }

    auto core = reinterpret_cast<PydrofoilCore*>(payload);
    //if(!core->sim_started)
    //    return 0;

    // If the dmi fails then we go the slow way otherwise we're done
    //if(core->use_dmi && 
    //    vcml::success(core->data.access_dmi(tlm::TLM_WRITE_COMMAND,address,&value,size,vcml::SBI_NONE)))
    //    return 0;

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
    if(address < 0x80000000 && address > 0x1fff){
        std::cout << "peripheral access" << std::endl;
        return 0;}
    if(address > 0x8fffffff || address < 0x1000){
        std::cout << "out of bound access" << std::endl;
        return 0;
    }

    auto core = reinterpret_cast<PydrofoilCore*>(payload);
    //if(!core->sim_started)
    //    return 0;
    //if(core->use_dmi && 
    //    vcml::success(core->data.access_dmi(tlm::TLM_READ_COMMAND,address,destination,size,vcml::SBI_NONE)))
    //    return 0;

    PydrofoilCore::MemAccess memtask;

    memtask.type = PydrofoilCore::MemTask::Read;
    memtask.addr = address; 
    memtask.size = size; // size sometimes appears too big...
    memtask.dest = destination;

    std::future<bool> res = memtask.result.get_future();

    {
        std::lock_guard lock(core->memtask_mutex);
        core->memtask_queue.push(std::move(memtask));
    }
    core->memtask_cv.notify_one();

    return res.get()? 0:1;
}