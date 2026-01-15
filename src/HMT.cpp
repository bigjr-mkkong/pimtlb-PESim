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

size_t SimdMemory::get_delay_cycl(size_t phys_addr) const {
    (void)phys_addr;
    return 0;
}
