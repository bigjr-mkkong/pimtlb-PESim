#include <algorithm>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include "HMT.h"
#include "libpimeval.h"

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

size_t SimdMemory::get_delay_cycl(size_t phys_addr, bool is_read, size_t cur_cycl, size_t pause_delay) {
    //pimPerfEnergyBank.cpp:1106 has the code
    //AutoDSE use offline timing model, but I need online method
    //I think it's better to borrow some basic functions like generateEvents/executeMemoryEvents, but write my own memory simulator
    //This can be easy since we only care part of the parameter

    /*
     * things to mind in thie small timing model:
     *  1. rowbuf hit/miss detection
     *  2. prec cannot appear immediatly after act finished
     */

    bool hit = dram_bank.is_hit(phys_addr);
    bool is_first = dram_bank.is_first_access();
    int prec_delay_slot = dram_bank.get_prec_delay(cur_cycl);

    size_t final_delay = 0, ddr_delay = 0;
    
    if(is_read) {
        if(hit) {
            // READ
            dram_bank.update_last_read(cur_cycl);
        } else if(is_first) {
            // ACT-READ
            ddr_delay =  0;//use executeMemoryEvents to get cycles of read, replace 0
            dram_bank.update_last_read(cur_cycl + ddr_delay);
        } else {
            // add delay slot in PREC-READ event
            // PREC-READ
            ddr_delay =  0;//use executeMemoryEvents to get cycles of read, replace 0
            dram_bank.update_last_read(cur_cycl + ddr_delay);
        }
    } else {

        if(hit) {
            // WRITE
            dram_bank.update_last_write(cur_cycl);
        } else if(is_first) {
            // ACT-WRITE
            ddr_delay = 0;//use executeMemoryEvents to get cycles of read, replace 0
            dram_bank.update_last_write(cur_cycl + ddr_delay);
        } else {
            //add delay slot in PREC-WRITE event(for precharge)
            //PREC-WRITE
            ddr_delay = 0;
            dram_bank.update_last_write(cur_cycl + ddr_delay);
        }
    }


    final_delay = pause_delay + prec_delay_slot + ddr_delay;
    // execute single ev with executeMemoryEvent();
    return final_delay;
}


bool tiny_dram_bank::is_hit(size_t paddr){
    long long row = paddr / sz_per_row;
    if(row == last_opened_row) return true;
    else {
        last_opened_row = row;
        return false;
    }
}
void tiny_dram_bank::update_last_read(size_t cycl){
    t_last_read = cycl;
}
void tiny_dram_bank::update_last_write(size_t cycl){
    t_last_write = cycl;
}
void tiny_dram_bank::update_last_act(size_t cycl){
    t_last_act = cycl;
}
bool tiny_dram_bank::is_first_access(){
    return last_opened_row == -1;
}
int tiny_dram_bank::get_prec_delay(size_t cycl){
    int max_delay = std::max({
                        tRTP - (cycl - t_last_read),
                        tWR - (cycl - t_last_write),
                        tRAS - (cycl - t_last_act)
    });

    return std::max(max_delay, 0);
}
