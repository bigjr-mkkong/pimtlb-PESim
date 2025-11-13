#include <memory>
#include <cstdio>
#include <cstdlib>
#include "HMT.h"
#include "libpimeval.h"


bool HMT_table_t::accept_fatptr(size_t varidx, size_t offset, HMT_flag_t flag) {
    auto val = hmt_table.find(varidx);

    if (val == hmt_table.end()){
        fprintf(stderr, "Varidx %d DNE in hmt table\n", varidx);
        return false;
    }

    if(val->second->get_max_len() < offset) {
        fprintf(stderr, "Varidx %d OOB access, offset: 0x%x \n", offset);
        return false;
    }

    if(val->second->get_flags() != flag) {
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
