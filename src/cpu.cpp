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
            PimObjId src0, src1, dst;
            int varid_dst = readed_inst.oprands[0].varidx;
            int varid_src0 = readed_inst.oprands[1].varidx;
            int varid_src1 = readed_inst.oprands[2].varidx;

            if(!hmt->accept_fatptr(varid_src0, 32, HMT_flag_t::RW)){
                fprintf(stderr, "cpu.cpp: failed to execute MUL\n");
                exit(1);
            } else {
                src0 = hmt->get_pim_obj_id(varid_src0);
            }

            if(!hmt->accept_fatptr(varid_src1, 32, HMT_flag_t::RW)){
                fprintf(stderr, "cpu.cpp: failed to execute MUL\n");
                exit(1);
            } else {
                src1 = hmt->get_pim_obj_id(varid_src1);
            }

            if(!hmt->accept_fatptr(varid_dst, 32, HMT_flag_t::RW)){
                fprintf(stderr, "cpu.cpp: failed to execute MUL\n");
                exit(1);
            } else {
                dst = hmt->get_pim_obj_id(varid_dst);
            }

#ifdef PIM_FUSE
            fused->add(pimAdd, src0, src1, dst);
#else
            PimStatus result = pimAdd(src0, src1, dst);
            assert(result == PIM_OK);
#endif
            printf("cycle %d decoded ADD\n", clock_cntr);
            break;
        }
        case Opcode::MUL: {
            PimObjId src0, src1, dst;
            int varid_dst = readed_inst.oprands[0].varidx;
            int varid_src0 = readed_inst.oprands[1].varidx;
            int varid_src1 = readed_inst.oprands[2].varidx;

            if(!hmt->accept_fatptr(varid_src0, 32, HMT_flag_t::RW)){
                fprintf(stderr, "cpu.cpp: failed to execute MUL\n");
                exit(1);
            } else {
                src0 = hmt->get_pim_obj_id(varid_src0);
            }

            if(!hmt->accept_fatptr(varid_src1, 32, HMT_flag_t::RW)){
                fprintf(stderr, "cpu.cpp: failed to execute MUL\n");
                exit(1);
            } else {
                src1 = hmt->get_pim_obj_id(varid_src1);
            }

            if(!hmt->accept_fatptr(varid_dst, 32, HMT_flag_t::RW)){
                fprintf(stderr, "cpu.cpp: failed to execute MUL\n");
                exit(1);
            } else {
                dst = hmt->get_pim_obj_id(varid_dst);
            }

#ifdef PIM_FUSE
            fused->add(pimMul, src0, src1, dst);
#else
            PimStatus result = pimMul(src0, src1, dst);
            assert(result == PIM_OK);
#endif
            printf("cycle %d decoded MUL\n", clock_cntr);
            break;
        }
        case Opcode::SCALED_ADD: {
            PimObjId src0, src1, dst;
            int varid_dst = readed_inst.oprands[0].varidx;
            int varid_src0 = readed_inst.oprands[1].varidx;
            int varid_src1 = readed_inst.oprands[2].varidx;
            int imm = readed_inst.imm0;

            if(!hmt->accept_fatptr(varid_src0, 32, HMT_flag_t::RW)){
                fprintf(stderr, "cpu.cpp: failed to execute MUL\n");
                exit(1);
            } else {
                src0 = hmt->get_pim_obj_id(varid_src0);
            }

            if(!hmt->accept_fatptr(varid_src1, 32, HMT_flag_t::RW)){
                fprintf(stderr, "cpu.cpp: failed to execute MUL\n");
                exit(1);
            } else {
                src1 = hmt->get_pim_obj_id(varid_src1);
            }

            if(!hmt->accept_fatptr(varid_dst, 32, HMT_flag_t::RW)){
                fprintf(stderr, "cpu.cpp: failed to execute MUL\n");
                exit(1);
            } else {
                dst = hmt->get_pim_obj_id(varid_dst);
            }

#ifdef PIM_FUSE
            fused->add(pimScaledAdd, src0, src1, dst, imm);
#else
            PimStatus result = pimScaledAdd(src0, src1, dst, imm);
            assert(result == PIM_OK);
#endif
            printf("cycle %d decoded SCALED_ADD\n", clock_cntr);
            break;
        }
        case Opcode::JUMP: {
            int target = readed_inst.imm0;
            if(target < 0 || target > imem->max_pc) {
                fprintf(stderr, "Invalid target pc\n");
                exit(1);
            }
            next_pc = target;
            need_jump = true;
            deco_flush = true;
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
    if (readed_inst_valid && !cpu_stop && !fl_hold) {
        deco_inst();
    }

    read_inst();

    next_pc = (need_jump)?next_pc:pc + 1;
    need_jump = false;
    next_clock_cntr = clock_cntr + 1;

    /*
     * Sequential logics
     */
    pc = next_pc;
    clock_cntr = next_clock_cntr;
    fl_hold = deco_flush;
    deco_flush = false;
}
