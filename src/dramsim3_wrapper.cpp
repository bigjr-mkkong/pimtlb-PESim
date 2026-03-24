#include "dramsim3_wrapper.h"
#include "pesim-configs.h"
#include "memory_system.h"
#include <cstdint>
#include <iostream>
#include <memory>


dramsim3_wrapper::dramsim3_wrapper(int ch, int ra, int bg, int ba){
    ms = std::make_unique<dramsim3::MemorySystem>(
            MEM_CONFIG_PATH,
            MEM_OUTPUT_PATH,
            [this] (uint64_t addr) {this->read_callback(addr);},
            [this] (uint64_t addr) {this->write_callback(addr);}
    );
    ms->SetPimMode(true);

    ch_ = ch;
    ra_ = ra;
    bg_ = bg;
    ba_ = ba;
}

bool dramsim3_wrapper::WillAcceptTransaction(uint64_t hex_addr, bool is_write) const {
    return ms->WillAcceptTransaction(hex_addr, is_write);
}
bool dramsim3_wrapper::AddTransaction(uint64_t hex_addr, bool is_write, bool is_pim) {
    uint64_t real_addr = ms->BankLocalToGlobalAddr(ch_, ra_, bg_, ba_, hex_addr);

    bool ret = ms->AddTransaction(real_addr, is_write, is_pim);

    if(ret){
        if(is_write)
            vis[real_addr].pend_write += 1;
        else
            vis[real_addr].pend_read += 1;
    }

    return ret;
}

void dramsim3_wrapper::ClockTick(){
    ms->ClockTick();
}


void dramsim3_wrapper::read_callback(uint64_t addr){
    if(vis[addr].pend_read <= 0) {
        std::cerr << "read callback failed: extra read callback" << std::endl;
        std::exit(1);
    } else {
        vis[addr].pend_read -= 1;
    }

    return;

}
void dramsim3_wrapper::write_callback(uint64_t addr){
    if(vis[addr].pend_write <= 0) {
        std::cerr << "write callback failed: extra write callback" << std::endl;
        std::exit(1);
    } else {
        vis[addr].pend_write -= 1;
    }

    return;
}

bool dramsim3_wrapper::drained() {
    for(auto &i: vis) {
        if(i.second.pend_read != 0 || i.second.pend_write != 0)
            return false;
    }

    return true;
}

int dramsim3_wrapper::get_pend_read(uint64_t addr){
    uint64_t real_addr = ms->BankLocalToGlobalAddr(ch_, ra_, bg_, ba_, addr);
    auto it = vis.find(real_addr);
    if(it == vis.end())
        return 0;
    else
        return it->second.pend_read;
}
int dramsim3_wrapper::get_pend_write(uint64_t addr){
    uint64_t real_addr = ms->BankLocalToGlobalAddr(ch_, ra_, bg_, ba_, addr);
    auto it = vis.find(real_addr);
    if(it == vis.end())
        return 0;
    else
        return it->second.pend_write;
}
