#ifndef __DRAMSIM3_WRAPPER_H__
#define __DRAMSIM3_WRAPPER_H__

#include "pesim-configs.h"

#include <cstdint>
#include <map>
#include <memory>
#include <unordered_map>


#include "memory_system.h"

class dramsim3_wrapper{
    public:
    struct vis_state_t{
        int pend_read = 0;
        int pend_write = 0;
    };
    dramsim3_wrapper();
    bool WillAcceptTransaction(uint64_t hex_addr, bool is_write) const;
    bool AddTransaction(uint64_t hex_addr, bool is_write, bool is_pim = false);
    void ClockTick();
    bool drained();
    int get_pend_read(uint64_t addr);
    int get_pend_write(uint64_t addr);

    private:
    std::unique_ptr<dramsim3::MemorySystem> ms;
    std::unordered_map<uint64_t, vis_state_t> vis;

    void read_callback(uint64_t addr);
    void write_callback(uint64_t addr);


};


#endif
