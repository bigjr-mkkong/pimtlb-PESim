#include <cassert>
#include <cstdio>
#include <cstdlib>
#include "cpu.h"
#include "libpimeval.h"


instruction_t &IMEM_t::get_by_pc(size_t pc){
    if(insts.size() <= pc) {
        fprintf(stderr, "Failed to fetch instuction in address: 0x%x\n", pc);
        exit(1);
    }
    return insts[pc];
}


void IMEM_t::push_inst(instruction_t &inst){
    insts.push_back(inst);
}


void cpu_t::read_inst(){
    instruction_t &inst = imem.get_by_pc(pc);
    readed_inst = inst;

    //tick();
}

void cpu_t::deco_inst(){
    switch(readed_inst.opcode) {
        case Opcode::LOAD: {

            int regidx = readed_inst.reg;
            fatptr ptr = readed_inst.oprands[0];
            size_t mem_offset;

            if(hmt.accept_fatptr(ptr.varidx, ptr.offset, ptr.flag)) {
                rf[regidx] = hmt.get_pim_obj_id(ptr.varidx);
                mem_offset = hmt.get_base_addr(ptr.varidx);
            } else {
                fprintf(stderr, "cpu failed\n");
                exit(1);
            }
            PimStatus result = pimCopyHostToDevice((void*)(mem.data() + mem_offset),\
                    rf[regidx]);
            assert(result == PIM_OK);
            break;
       }
        case Opcode::STORE: {
            int regidx = readed_inst.reg;
            fatptr ptr = readed_inst.oprands[0];
            size_t mem_offset;

            if(hmt.accept_fatptr(ptr.varidx, ptr.offset, ptr.flag)) {
                rf[regidx] = hmt.get_pim_obj_id(ptr.varidx);
                mem_offset = hmt.get_base_addr(ptr.varidx);
            } else {
                fprintf(stderr, "cpu failed\n");
                exit(1);
            }
            PimStatus result = pimCopyDeviceToHost(rf[regidx], \
                    (void*)(mem.data() + mem_offset));
            assert(result == PIM_OK);

            break;
       }
    }
}
