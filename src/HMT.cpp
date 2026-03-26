#include <algorithm>
#include <iostream>
#include <string>
#include <memory>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include "HMT.h"
#include "dramsim3_wrapper.h"
#include "memory_system.h"
// #include "libpimeval.h"

// #include "../../src/pimSim.h"
//
SimdMemory::SimdMemory(int ch, int ra, int bg, int ba) {
    ch_ = ch;
    ra_ = ra;
    bg_ = bg;
    ba_ = ba;
    dsim3 = std::make_unique<dramsim3_wrapper>(ch_, ra_, bg_, ba_, true);
}

const SimdMemory::Region *SimdMemory::find_region(size_t phys_addr) const {
    for (const auto &entry : regions_) {
        const auto &region = entry.second;
        if (phys_addr >= region.base_addr &&
            phys_addr + 16 <= region.base_addr + region.data.size()) {
            return &region;
        }
    }
    return nullptr;
}

SimdMemory::Region *SimdMemory::find_region(size_t phys_addr) {
    for (auto &entry : regions_) {
        auto &region = entry.second;
        if (phys_addr >= region.base_addr &&
            phys_addr + 16 <= region.base_addr + region.data.size()) {
            return &region;
        }
    }
    return nullptr;
}


void SimdMemory::add_region(uint32_t varidx, size_t size_bytes) {
    size_t base_addr = 0;
    for (const auto &entry : regions_) {
        base_addr = std::max(base_addr, entry.second.base_addr + entry.second.data.size());
    }
    regions_[varidx] = Region{base_addr, std::vector<uint8_t>(size_bytes, 0)};
}

void SimdMemory::fill_region(uint32_t varidx, const std::vector<uint8_t> &src){
    auto &region = regions_.at(varidx);

    if(src.size() != region.data.size())
        throw std::out_of_range(\
                "fill_region() destination size should be the same as source");

    region.data.assign(src.begin(), src.end());

}

bool SimdMemory::check_fatptr(const SimdFatptr &ptr, size_t size_bytes) const {
    auto it = regions_.find(ptr.varidx);
    if (it == regions_.end()) {
        return false;
    }
    if (ptr.offset < 0) {
        return false;
    }
    size_t offset = static_cast<size_t>(ptr.offset);
    if (offset + size_bytes > it->second.data.size()) {
        return false;
    }
    return true;
}

size_t SimdMemory::translate_fatptr(const SimdFatptr &ptr, size_t size_bytes) const {
    if (!check_fatptr(ptr, size_bytes)) {
        throw std::out_of_range("invalid fatptr translation");
    }
    const auto &region = regions_.at(ptr.varidx);
    return region.base_addr + static_cast<size_t>(ptr.offset);
}

std::array<uint32_t, 4> SimdMemory::load128(size_t phys_addr) const {
    const auto *region = find_region(phys_addr);
    if (!region) {
        throw std::out_of_range("invalid fatptr load");
    }
    std::array<uint32_t, 4> result{};
    size_t offset = phys_addr - region->base_addr;
    std::memcpy(result.data(), region->data.data() + offset, 16);
    return result;
}

void SimdMemory::store128(size_t phys_addr, const std::array<uint32_t, 4> &value) {
    auto *region = find_region(phys_addr);
    if (!region) {
        throw std::out_of_range("invalid fatptr store");
    }
    size_t offset = phys_addr - region->base_addr;
    std::memcpy(region->data.data() + offset, value.data(), 16);
}

bool SimdMemory::equal128(size_t phys_addr, const std::array<uint32_t, 4> &value) const {
    const auto *region = find_region(phys_addr);
    if (!region) {
        return false;
    }
    size_t offset = phys_addr - region->base_addr;
    return std::memcmp(region->data.data() + offset, value.data(), 16) == 0;
}

size_t SimdMemory::get_delay_cycl(size_t phys_addr, bool is_read, size_t cur_cycl) {
    //pimPerfEnergyBank.cpp:1106 has the code
    //AutoDSE use offline timing model, but I need online method
    //I think it's better to borrow some basic functions like generateEvents/executeMemoryEvents, but write my own memory simulator
    //This can be easy since we only care part of the parameter

    /*
     * things to mind in thie small timing model:
     *  1. rowbuf hit/miss detection
     *  2. prec cannot appear immediatly after act finished
     */

    pimeval::EventNode *ev;

    bool hit = dram_bank.is_hit(phys_addr);
    bool is_active = dram_bank.is_active(phys_addr);
    dram_bank.update_hit(phys_addr);
    size_t ddr_delay = 0;
    
    if(is_read) {
        if(hit) {
            // READ
            ev = generateEvent(pimeval::EventType::READ_SRC1, 0, 0, 0, 0, 0, 0, 0);
            dram_bank.push_ddr(ev);

            ddr_delay = dram_bank.executeMemoryEvent(cur_cycl);

        } else if(!is_active) {
            // ACT-READ
            dram_bank.push_ddr(generateEvent(pimeval::EventType::ACTIVATE_READ, 0, 0, 0, 0, 0, 0, 0));
            ev = generateEvent(pimeval::EventType::READ_SRC1, 0, 0, 0, 0, 0, 0, 0);
            dram_bank.push_ddr(ev);
            ddr_delay = dram_bank.executeMemoryEvent(cur_cycl);

        } else {
            // add delay slot in PREC-READ event
            // PREC-READ
#ifndef MASA_TLDRAM
            ev = generateEvent(pimeval::EventType::PRECHARGE_READ, 0, 0, 0, 0, 0, 0, 0);
            dram_bank.push_ddr(ev);

            ev = generateEvent(pimeval::EventType::ACTIVATE_READ, 0, 0, 0, 0, 0, 0, 0);
            dram_bank.push_ddr(ev);

            ev = generateEvent(pimeval::EventType::READ_SRC1, 0, 0, 0, 0, 0, 0, 0);
            dram_bank.push_ddr(ev);
#else
            ev = generateEvent(pimeval::EventType::FAST_PREC, 0, 0, 0, 0, 0, 0, 0);
            dram_bank.push_ddr(ev);

            ev = generateEvent(pimeval::EventType::ISO_OPEN_CLOSE, 0, 0, 0, 0, 0, 0, 0);
            dram_bank.push_ddr(ev);

            ev = generateEvent(pimeval::EventType::ACTIVATE_READ, 0, 0, 0, 0, 0, 0, 0);
            dram_bank.push_ddr(ev);

            ev = generateEvent(pimeval::EventType::ISO_OPEN_CLOSE, 0, 0, 0, 0, 0, 0, 0);
            dram_bank.push_ddr(ev);

            ev = generateEvent(pimeval::EventType::READ_SRC1, 0, 0, 0, 0, 0, 0, 0);
            dram_bank.push_ddr(ev);
#endif
            ddr_delay = dram_bank.executeMemoryEvent(cur_cycl);
        }
    } else {

        if(hit) {
            // WRITE
            ddr_delay = 0;
            ev = generateEvent(pimeval::EventType::WRITE_CHUNK, 0, 0, 0, 0, 0, 0, 0);
            dram_bank.push_ddr(ev);

            ddr_delay = dram_bank.executeMemoryEvent(cur_cycl);
        } else if(!is_active) {
            // ACT-WRITE
            ddr_delay = 0;
            dram_bank.push_ddr(generateEvent(pimeval::EventType::ACTIVATE_READ, 0, 0, 0, 0, 0, 0, 0));
            ev = generateEvent(pimeval::EventType::WRITE_CHUNK, 0, 0, 0, 0, 0, 0, 0);
            dram_bank.push_ddr(ev);

            ddr_delay = dram_bank.executeMemoryEvent(cur_cycl);
        } else {
            //PREC-WRITE
#ifndef MASA_TLDRAM
            ev = generateEvent(pimeval::EventType::PRECHARGE_WRITE, 0, 0, 0, 0, 0, 0, 0);
            dram_bank.push_ddr(ev);

            ev = generateEvent(pimeval::EventType::ACTIVATE_WRITE, 0, 0, 0, 0, 0, 0, 0);
            dram_bank.push_ddr(ev);

            ev = generateEvent(pimeval::EventType::WRITE_CHUNK, 0, 0, 0, 0, 0, 0, 0);
            dram_bank.push_ddr(ev);
#else
            ev = generateEvent(pimeval::EventType::FAST_PREC, 0, 0, 0, 0, 0, 0, 0);
            dram_bank.push_ddr(ev);

            ev = generateEvent(pimeval::EventType::ISO_OPEN_CLOSE, 0, 0, 0, 0, 0, 0, 0);
            dram_bank.push_ddr(ev);

            ev = generateEvent(pimeval::EventType::ACTIVATE_WRITE, 0, 0, 0, 0, 0, 0, 0);
            dram_bank.push_ddr(ev);

            ev = generateEvent(pimeval::EventType::ISO_OPEN_CLOSE, 0, 0, 0, 0, 0, 0, 0);
            dram_bank.push_ddr(ev);

            ev = generateEvent(pimeval::EventType::WRITE_CHUNK, 0, 0, 0, 0, 0, 0, 0);
            dram_bank.push_ddr(ev);

#endif

            ddr_delay = dram_bank.executeMemoryEvent(cur_cycl);
        }
    }


    return ddr_delay - 1 < 0?0:ddr_delay - 1;
}


size_t SimdMemory::get_delay_cycl_dramsim3(size_t phys_addr, bool is_read) {
    size_t ticks = 0;
    bool is_write = !is_read;
    if(is_read) {
        while (dsim3->get_pend_write(phys_addr, true) != 0 || !dsim3->WillAcceptTransaction(phys_addr, false)) {
            ticks++;
            dsim3->ClockTick();
        }

        bool ok = dsim3->AddTransaction(phys_addr, is_write, true);
        if(!ok){
            std::cerr<<"Failed to add transaction of address: "<<phys_addr<<std::endl;
        }

        while (dsim3->get_pend_read(phys_addr, true) != 0) {
            ticks++;
            dsim3->ClockTick();
        }

    } else {
        while(!dsim3->WillAcceptTransaction(phys_addr, is_write)){
            ticks++;
            dsim3->ClockTick();
        }
        bool ok = dsim3->AddTransaction(phys_addr, is_write, true);
        if(!ok){
            std::cerr<<"Failed to add transaction of address: "<<phys_addr<<std::endl;
        }
    }


    return ticks;
}

void SimdMemory::reset() {
    dram_bank.reset();
    regions_.clear();

    dsim3 = std::make_unique<dramsim3_wrapper>(ch_, ra_, bg_, ba_, true);
    return;
}

tiny_dram_bank &SimdMemory::bank_model(){
    return dram_bank;
}

bool tiny_dram_bank::is_hit(size_t paddr){
    size_t SA = paddr / sz_per_SA;
    size_t row = (paddr%sz_per_SA) / sz_per_row;
#ifdef MASA_TLDRAM
    if(sa_sel_table[SA] == row) return true;
    else {
        sa_sel_table[SA] = row;
        return false;
    }
#else
    if(opened_row == row) return true;
    else {
        opened_row = row;
        opened_SA = SA;
        return false;
    }
#endif
}
void tiny_dram_bank::update_hit(size_t paddr){
    size_t SA = paddr / sz_per_SA;
    size_t row = (paddr%sz_per_SA) / sz_per_row;
    sa_sel_table[SA] = row;
    
    return;
}
bool tiny_dram_bank::is_active(size_t paddr){
    size_t SA = paddr / sz_per_SA;
    return sa_sel_table[SA] == -1;
}
int tiny_dram_bank::get_prec_delay(size_t cycl){
    int max_delay = std::max({
                        tRTP - (cycl - t_last_read),
                        tRAS - (cycl - t_last_act)
    });

    return std::max(max_delay, 0);
}

int tiny_dram_bank::get_rd_delay(size_t cycl){
#ifndef MASA_TLDRAM
    int max_delay = tWTR - (cycl - t_last_write);
#else
    int max_delay = tWTR_FAST - (cycl - t_last_write);
#endif

    return std::max(max_delay, 0);
}

int tiny_dram_bank::get_wr_delay(size_t cycl){
#ifndef MASA_TLDRAM
    int max_delay = tWR - (cycl - t_last_write);
#else
    int max_delay = tWR_FAST - (cycl - t_last_write);
#endif

    return std::max(max_delay, 0);
}

void tiny_dram_bank::push_ddr(pimeval::EventNode *ev){
    ddr_events.push(*ev);

    return;
}

int tiny_dram_bank::executeMemoryEvent(size_t currCycle){
    int total_cycle_required = 0, single_cycle_required = 0;

    size_t sim_cycl = currCycle; 
    while(!ddr_events.empty()){
        pimeval::EventNode ev = ddr_events.front();
        ddr_events.pop();

        switch (ev.type)
        {
        case pimeval::EventType::ACTIVATE_READ:
        case pimeval::EventType::ACTIVATE_WRITE:
        {  
            ev.stalledCycle = 0;
            ev.cycleCount = tRCDRD;
            single_cycle_required = tRCDRD;
            t_last_act = sim_cycl + single_cycle_required;
            break;
        }
        case pimeval::EventType::PRECHARGE_READ:
        case pimeval::EventType::PRECHARGE_WRITE:
        {
            ev.stalledCycle = get_prec_delay(sim_cycl);
            ev.cycleCount = tRP + ev.stalledCycle;
            single_cycle_required = tRP + ev.stalledCycle;
            break;
        }
        case pimeval::EventType::READ_SRC1:
        case pimeval::EventType::READ_SRC2:
        case pimeval::EventType::READ_SCALAR:
        {
            ev.stalledCycle = get_rd_delay(sim_cycl);
            ev.cycleCount = tCCDL + ev.stalledCycle; 
            single_cycle_required = tCCDL + ev.stalledCycle;
            t_last_read = sim_cycl + single_cycle_required;
            break;
        }
        case pimeval::EventType::WRITE_CHUNK:
        {
            ev.stalledCycle = get_wr_delay(sim_cycl);
            ev.cycleCount = tCCDL + ev.stalledCycle;
            single_cycle_required = tCCDL + ev.stalledCycle;
            t_last_write = sim_cycl + single_cycle_required;
            break;
        }
#ifdef MASA_TLDRAM
        case pimeval::EventType::FAST_PREC:
        {
            ev.stalledCycle = get_prec_delay(sim_cycl);
            ev.cycleCount = tRP_FAST + ev.stalledCycle;
            single_cycle_required = tRP_FAST + ev.stalledCycle;
            t_last_write = sim_cycl + single_cycle_required;
        }
        case pimeval::EventType::ISO_OPEN_CLOSE:
        {
            ev.stalledCycle = 0;
            ev.cycleCount = tISO + ev.stalledCycle;
            single_cycle_required = tISO + ev.stalledCycle;
            t_last_write = sim_cycl + single_cycle_required;
        }
#endif
        default:
            break;
        }
        total_cycle_required += single_cycle_required;
        sim_cycl += single_cycle_required;
    }
    return total_cycle_required;
}

void tiny_dram_bank::reset() {
    /* Reset timing parameter back to its original value */
    tRAS = 52;
    tRTP = 12;
    tWR = 24; 
    tWTR = 3; 
    tRCDRD = 22;
    tRP = 22;
    tCCDL = 8;
    tSA_SEL = 3;
    tRP_FAST = 12;
    tISO = 2;
    tWR_FAST = 12;
    tWTR_FAST = 0;

    t_last_act = 0;
    t_last_read = 0;
    t_last_write = 0;

    for(int i=0; i<256; i++)
        sa_sel_table[i] = -1;

    opened_row = -1;
    opened_SA = -1;

    while(!ddr_events.empty())
        ddr_events.pop();
}
