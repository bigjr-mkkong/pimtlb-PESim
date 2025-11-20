#include <cassert>
#include <cstdio>
#include <cstdlib>
#include "cpu.h"
#include "HMT.h"
#include "libpimeval.h"


instruction_t &IMEM_t::get_by_pc(size_t pc){
    if(insts.size() < pc) {
        fprintf(stderr, "Failed to fetch instuction in address: 0x%x\n", pc);
        exit(1);
    }
    return insts[pc];
}


void IMEM_t::push_inst(instruction_t &inst){
    insts.push_back(inst);
}


void cpu_t::read_inst(){
    if(imem->insts.size() == pc){
        readed_inst_valid = false;
        return;
    }

    instruction_t &inst = imem->get_by_pc(pc);
    readed_inst = inst;
    readed_inst_valid = true;

    printf("cycle %d issued inst\n", clock_cntr);

    return;
}

void cpu_t::deco_inst(){
    switch(readed_inst.opcode) {
        case Opcode::ADD: {
            int varid_0 = readed_inst.oprands[0].varidx;
            int varid_1 = readed_inst.oprands[1].varidx;
            if(!hmt->accept_fatptr(varid_0, 32, HMT_flag_t::RW)){
                fprintf(stderr, "cpu.cpp: failed to execute MUL\n");
                exit(1);
            }
            PimObjId arg_1 = hmt->get_pim_obj_id(varid_0);
            if(!hmt->accept_fatptr(varid_1, 32, HMT_flag_t::RW)){
                fprintf(stderr, "cpu.cpp: failed to execute MUL\n");
                exit(1);
            }
            PimObjId arg_2 = hmt->get_pim_obj_id(varid_1);
#ifdef PIM_FUSE
            fused->add(pimAdd, arg_1, arg_2, arg_1);
#else
            PimStatus result = pimAdd(arg_1, arg_2, arg_1);
            assert(result == PIM_OK);
#endif
            printf("cycle %d decoded ADD\n", clock_cntr);
            break;
        }
        case Opcode::MUL: {
            int varid_0 = readed_inst.oprands[0].varidx;
            int varid_1 = readed_inst.oprands[1].varidx;
            if(!hmt->accept_fatptr(varid_0, 32, HMT_flag_t::RW)){
                fprintf(stderr, "cpu.cpp: failed to execute MUL\n");
                exit(1);
            }
            PimObjId arg_1 = hmt->get_pim_obj_id(varid_0);
            if(!hmt->accept_fatptr(varid_1, 32, HMT_flag_t::RW)){
                fprintf(stderr, "cpu.cpp: failed to execute MUL\n");
                exit(1);
            }
            PimObjId arg_2 = hmt->get_pim_obj_id(varid_1);
#ifdef PIM_FUSE
            fused->add(pimMul, arg_1, arg_2, arg_1);
#else
            PimStatus result = pimMul(arg_1, arg_2, arg_1);
            assert(result == PIM_OK);
#endif
            printf("cycle %d decoded MUL\n", clock_cntr);
            break;
        }
        case Opcode::EXIT: {
            cpu_stop = true;
            printf("cycle %d decoded EXIT\n", clock_cntr);
            break;
        }
        case Opcode::NOP: {
            printf("cycle %d decoded NOP\n", clock_cntr);
            break;
        }
    }
}

void cpu_t::tick(){
    if (readed_inst_valid && !cpu_stop) {
        deco_inst();
    }

    read_inst();

    next_pc = pc + 1;
    next_clock_cntr = clock_cntr + 1;

    /*
     * Sequential logics
     */
    pc = next_pc;
    clock_cntr = next_clock_cntr;
}
