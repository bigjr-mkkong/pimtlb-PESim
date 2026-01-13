#include <cstring>
#include <memory>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include "HMT.h"
#include "libpimeval.h"


bool HMT_table_t::accept_fatptr(size_t varidx, size_t offset, HMT_flag_t flag) {
    // auto val = hmt_table.find(varidx);
    auto val = hmt_table.find(varidx);

    if (val == hmt_table.end()){
        fprintf(stderr, "Varidx %d DNE in hmt table\n", varidx);
        return false;
    }

    if(val->second->get_max_len() < offset) {
        fprintf(stderr, "Varidx %d OOB access, offset: 0x%x \n", offset);
        return false;
    }

    if(val->second->get_flags() != HMT_flag_t::RW &&\
            val->second->get_flags() != flag) {
        fprintf(stderr, "Varidx %d invalid flag\n");
        return false;
    }

    return true;

}


PimObjId HMT_table_t::get_pim_obj_id(size_t varidx){
    return hmt_table[varidx]->get_memid();
}

size_t HMT_table_t::get_base_addr(size_t varidx){
    return hmt_table[varidx]->get_base_addr();
}

void HMT_table_t::add_new_ent(size_t base_addr, size_t varidx, size_t max_len, PimObjId mem_id, HMT_flag_t access_flag){
    hmt_table[varidx] = std::make_unique<HMT_entry_t>(base_addr, max_len, mem_id, access_flag);
    return;
}

void SimdMemory::add_region(uint32_t varidx, size_t size_bytes) {
    regions_[varidx] = std::vector<uint8_t>(size_bytes, 0);
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
    if (offset + size_bytes > it->second.size()) {
        return false;
    }
    return true;
}

std::array<uint32_t, 4> SimdMemory::load128(const SimdFatptr &ptr) const {
    if (!check_fatptr(ptr, 16)) {
        throw std::out_of_range("invalid fatptr load");
    }
    std::array<uint32_t, 4> result{};
    const auto &region = regions_.at(ptr.varidx);
    std::memcpy(result.data(), region.data() + ptr.offset, 16);
    return result;
}

void SimdMemory::store128(const SimdFatptr &ptr, const std::array<uint32_t, 4> &value) {
    if (!check_fatptr(ptr, 16)) {
        throw std::out_of_range("invalid fatptr store");
    }
    auto &region = regions_.at(ptr.varidx);
    std::memcpy(region.data() + ptr.offset, value.data(), 16);
}

bool SimdMemory::equal128(const SimdFatptr &ptr, const std::array<uint32_t, 4> &value) const {
    if (!check_fatptr(ptr, 16)) {
        return false;
    }
    const auto &region = regions_.at(ptr.varidx);
    return std::memcmp(region.data() + ptr.offset, value.data(), 16) == 0;
}
