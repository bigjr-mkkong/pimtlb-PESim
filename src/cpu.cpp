#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
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
        cpu_stop = true;
        return;
    }

    instruction_t &inst = imem->get_by_pc(pc);
    readed_inst = inst;

    printf("cycle %d issued inst\n", clock_cntr);

    return;
}

void cpu_t::deco_inst(){
    IFID_HMT_valid = false;
    switch(readed_inst.opcode) {
        case Opcode::ADD: {
            // PimObjId src0, src1, dst;
            IFID_HMT_varid_dst = readed_inst.oprands[0].varidx;
            IFID_HMT_varid_src0 = readed_inst.oprands[1].varidx;
            IFID_HMT_varid_src1 = readed_inst.oprands[2].varidx;
            IFID_HMT_opc = Opcode::ADD;
            IFID_HMT_valid = true;

            // if(!hmt->accept_fatptr(varid_src0, 32, HMT_flag_t::RW)){
            //     fprintf(stderr, "cpu.cpp: failed to execute MUL\n");
            //     exit(1);
            // } else {
            //     src0 = hmt->get_pim_obj_id(varid_src0);
            // }

            // if(!hmt->accept_fatptr(varid_src1, 32, HMT_flag_t::RW)){
            //     fprintf(stderr, "cpu.cpp: failed to execute MUL\n");
            //     exit(1);
            // } else {
            //     src1 = hmt->get_pim_obj_id(varid_src1);
            // }

            // if(!hmt->accept_fatptr(varid_dst, 32, HMT_flag_t::RW)){
            //     fprintf(stderr, "cpu.cpp: failed to execute MUL\n");
            //     exit(1);
            // } else {
            //     dst = hmt->get_pim_obj_id(varid_dst);
            // }

// #ifdef PIM_FUSE
            // fused->add(pimAdd, src0, src1, dst);
// #else
            // PimStatus result = pimAdd(src0, src1, dst);
            // assert(result == PIM_OK);
// #endif
            // printf("cycle %d decoded ADD\n", clock_cntr);
            break;
        }
        case Opcode::MUL: {
            // PimObjId src0, src1, dst;
            IFID_HMT_varid_dst = readed_inst.oprands[0].varidx;
            IFID_HMT_varid_src0 = readed_inst.oprands[1].varidx;
            IFID_HMT_varid_src1 = readed_inst.oprands[2].varidx;
            IFID_HMT_opc = Opcode::MUL;
            IFID_HMT_valid = true;
            break;
        }
        case Opcode::SCALED_ADD: {
            // PimObjId src0, src1, dst;
            IFID_HMT_varid_dst = readed_inst.oprands[0].varidx;
            IFID_HMT_varid_src0 = readed_inst.oprands[1].varidx;
            IFID_HMT_varid_src1 = readed_inst.oprands[2].varidx;
            IFID_HMT_imm0 = readed_inst.imm0;
            IFID_HMT_opc = Opcode::SCALED_ADD;
            IFID_HMT_valid = true;

            // if(!hmt->accept_fatptr(varid_src0, 32, HMT_flag_t::RW)){
            //     fprintf(stderr, "cpu.cpp: failed to execute MUL\n");
            //     exit(1);
            // } else {
            //     src0 = hmt->get_pim_obj_id(varid_src0);
            // }

            // if(!hmt->accept_fatptr(varid_src1, 32, HMT_flag_t::RW)){
            //     fprintf(stderr, "cpu.cpp: failed to execute MUL\n");
            //     exit(1);
            // } else {
            //     src1 = hmt->get_pim_obj_id(varid_src1);
            // }

            // if(!hmt->accept_fatptr(varid_dst, 32, HMT_flag_t::RW)){
            //     fprintf(stderr, "cpu.cpp: failed to execute MUL\n");
            //     exit(1);
            // } else {
            //     dst = hmt->get_pim_obj_id(varid_dst);
            // }

// #ifdef PIM_FUSE
            // fused->add(pimScaledAdd, src0, src1, dst, imm);
// #else
            // PimStatus result = pimScaledAdd(src0, src1, dst, imm);
            // assert(result == PIM_OK);
// #endif
            // printf("cycle %d decoded SCALED_ADD\n", clock_cntr);
            break;
        }
        case Opcode::BOUND_JUMP: {
            int target = readed_inst.imm0;
            if(target < 0 || target > imem->max_pc) {
                fprintf(stderr, "Invalid target pc\n");
                exit(1);
            }
            next_pc = target;
            need_jump = true;
            break;
        }
        case Opcode::EXIT: {
            cpu_stop = true;
            IFID_HMT_valid = false;
            printf("cycle %d decoded EXIT\n", clock_cntr);
            break;
        }
        case Opcode::NOP: {
            printf("cycle %d decoded NOP\n", clock_cntr);
            IFID_HMT_opc = Opcode::NOP;
            IFID_HMT_valid = true;
            break;
        }
    }
}

void cpu_t::hmt_check(){
    /*
     * TODO:
     * implement hmt check in independent stage, and pipe down result
     * into next RCW(Read-Compute-Write) stage
     */

    HMT_RCW_valid = false;
    if(IFID_HMT_opc == Opcode::ADD || IFID_HMT_opc == Opcode::MUL || IFID_HMT_opc == Opcode::SCALED_ADD) {
        if(!hmt->accept_fatptr(IFID_HMT_varid_src0, 32, HMT_flag_t::RW)){
            fprintf(stderr, "cpu.cpp: failed to execute MUL\n");
            exit(1);
        } else {
            HMT_RCW_src0 = hmt->get_pim_obj_id(IFID_HMT_varid_src0);
        }

        if(!hmt->accept_fatptr(IFID_HMT_varid_src1, 32, HMT_flag_t::RW)){
            fprintf(stderr, "cpu.cpp: failed to execute MUL\n");
            exit(1);
        } else {
            HMT_RCW_src1 = hmt->get_pim_obj_id(IFID_HMT_varid_src1);
        }

        if(!hmt->accept_fatptr(IFID_HMT_varid_dst, 32, HMT_flag_t::RW)){
            fprintf(stderr, "cpu.cpp: failed to execute MUL\n");
            exit(1);
        } else {
            HMT_RCW_dst = hmt->get_pim_obj_id(IFID_HMT_varid_dst);
        }

        HMT_RCW_imm0 = (IFID_HMT_opc == Opcode::SCALED_ADD)?IFID_HMT_imm0:0;
    }

    HMT_RCW_opc = IFID_HMT_opc;
    HMT_RCW_valid = true;

    return;
}


void cpu_t::RCW(){
    switch(HMT_RCW_opc){
        case Opcode::NOP:
        {
            fprintf(stdout, "NOP in RCW stage\n");
            break;
        }

        case Opcode::ADD:
        {
#ifdef PIM_FUSE
            fused->add(pimAdd, HMT_RCW_src0, HMT_RCW_src1, HMT_RCW_dst);
#else
            PimStatus result = pimAdd(HMT_RCW_src0, HMT_RCW_src1, HMT_RCW_dst);
            assert(result == PIM_OK);
#endif
            break;
        }

        case Opcode::MUL:
        {
#ifdef PIM_FUSE
            fused->add(pimMul, HMT_RCW_src0, HMT_RCW_src1, HMT_RCW_dst);
#else
            PimStatus result = pimMul(HMT_RCW_src0, HMT_RCW_src1, HMT_RCW_dst);
            assert(result == PIM_OK);
#endif
            break;
        }

        case Opcode::SCALED_ADD:
        {

#ifdef PIM_FUSE
            fused->add(pimScaledAdd, HMT_RCW_src0, HMT_RCW_src1, HMT_RCW_dst,\
                    HMT_RCW_imm0);
#else
            PimStatus result = pimScaledAdd(HMT_RCW_src0, HMT_RCW_src1,\
                    HMT_RCW_dst, HMT_RCW_imm0);
            assert(result == PIM_OK);
#endif
            break;
        }

        default:
        {
            fprintf(stderr, "Unregocnizable Opcode in RCW stage: %d\n",\
                    HMT_RCW_imm0);
            exit(1);
        }
        
    }
    return;
}

bool cpu_t::stopable(){
    return !IFID_HMT_valid && !HMT_RCW_valid;
}

void cpu_t::tick(){
    if(HMT_RCW_valid){
        RCW();
    }

    if(IFID_HMT_valid){
        hmt_check();
    } else {
        HMT_RCW_valid = false;
    }

    if (!cpu_stop) {
        IFID_HMT_valid = false;
        read_inst();
        deco_inst();
    }

    next_pc = (need_jump)?next_pc:pc + 1;
    need_jump = false;
    next_clock_cntr = clock_cntr + 1;

    /*
     * Sequential logics
     */
    pc = next_pc;
    clock_cntr = next_clock_cntr;
}

SimdCpu::SimdCpu(SimdMemory *memory) : memory_(memory) {
    assert(memory_ != nullptr);
}

void SimdCpu::load_program(const std::vector<SimdInstruction> &program) {
    program_ = program;
    pc_ = 0;
    cpu_stop_ = false;
    if_id_ = {};
    id_hmt_ = {};
    hmt_ex_ = {};
    ex_mem_ = {};
    mem_wb_ = {};
}

bool SimdCpu::is_stopped() const {
    return cpu_stop_;
}

const std::array<uint32_t, 4> &SimdCpu::get_vreg(size_t idx) const {
    if (idx >= kVectorRegisters) {
        throw std::out_of_range("vector register index out of range");
    }
    return vregs_[idx];
}

void SimdCpu::set_vreg(size_t idx, const std::array<uint32_t, 4> &value) {
    if (idx >= kVectorRegisters) {
        throw std::out_of_range("vector register index out of range");
    }
    vregs_[idx] = value;
}

SimdFatptr SimdCpu::get_freg(size_t idx) const {
    if (idx >= kFatptrRegisters) {
        throw std::out_of_range("fatptr register index out of range");
    }
    return fregs_[idx];
}

void SimdCpu::set_freg(size_t idx, const SimdFatptr &value) {
    if (idx >= kFatptrRegisters) {
        throw std::out_of_range("fatptr register index out of range");
    }
    fregs_[idx] = value;
}

bool SimdCpu::uses_fatptr(const SimdInstruction &inst) const {
    switch (inst.opcode) {
        case SimdOpcode::Ld128:
        case SimdOpcode::St128:
        case SimdOpcode::EqualExit:
        case SimdOpcode::FatptrAdd4:
        case SimdOpcode::FatptrSub4:
            return true;
        case SimdOpcode::FatptrLi:
            return true;
        case SimdOpcode::Add128:
        case SimdOpcode::Jump:
        case SimdOpcode::Nop:
            return false;
    }
    return false;
}

void SimdCpu::validate_reg_index(int idx, size_t max, const char *name) const {
    if (idx < 0 || static_cast<size_t>(idx) >= max) {
        throw std::out_of_range(name);
    }
}

void SimdCpu::tick() {
    auto resolve_vec_operand = [&](int idx) -> std::array<uint32_t, 4> {
        validate_reg_index(idx, kVectorRegisters, "vector register out of range");
        if (ex_mem_.valid && ex_mem_.inst.rd == idx) {
            if (ex_mem_.inst.opcode == SimdOpcode::Add128) {
                return ex_mem_.vec_result;
            }
        }
        if (mem_wb_.valid && mem_wb_.inst.rd == idx) {
            if (mem_wb_.inst.opcode == SimdOpcode::Add128 ||
                mem_wb_.inst.opcode == SimdOpcode::Ld128) {
                return mem_wb_.vec_result;
            }
        }
        return vregs_[idx];
    };

    if (mem_wb_.valid) {
        const auto &inst = mem_wb_.inst;
        switch (inst.opcode) {
            case SimdOpcode::Add128:
                validate_reg_index(inst.rd, kVectorRegisters, "vector register out of range");
                vregs_[inst.rd] = mem_wb_.vec_result;
                break;
            case SimdOpcode::Ld128:
                validate_reg_index(inst.rd, kVectorRegisters, "vector register out of range");
                vregs_[inst.rd] = mem_wb_.vec_result;
                break;
            case SimdOpcode::FatptrLi:
            case SimdOpcode::FatptrAdd4:
            case SimdOpcode::FatptrSub4:
                validate_reg_index(inst.frd, kFatptrRegisters, "fatptr register out of range");
                fregs_[inst.frd] = mem_wb_.fatptr_result;
                break;
            case SimdOpcode::EqualExit:
                if (mem_wb_.should_stop) {
                    cpu_stop_ = true;
                }
                break;
            case SimdOpcode::St128:
            case SimdOpcode::Jump:
            case SimdOpcode::Nop:
                break;
        }
    }

    MemWb next_mem_wb{};
    if (ex_mem_.valid) {
        next_mem_wb.valid = true;
        next_mem_wb.inst = ex_mem_.inst;
        switch (ex_mem_.inst.opcode) {
            case SimdOpcode::Ld128:
                next_mem_wb.vec_result = memory_->load128(ex_mem_.fatptr);
                break;
            case SimdOpcode::St128:
                memory_->store128(ex_mem_.fatptr, ex_mem_.vec_operand);
                break;
            case SimdOpcode::Add128:
                next_mem_wb.vec_result = ex_mem_.vec_result;
                break;
            case SimdOpcode::EqualExit:
                next_mem_wb.should_stop = memory_->equal128(ex_mem_.fatptr, ex_mem_.vec_operand);
                break;
            case SimdOpcode::FatptrLi:
            case SimdOpcode::FatptrAdd4:
            case SimdOpcode::FatptrSub4:
                next_mem_wb.fatptr_result = ex_mem_.fatptr_result;
                break;
            case SimdOpcode::Jump:
            case SimdOpcode::Nop:
                break;
        }
    }

    ExMem next_ex_mem{};
    if (hmt_ex_.valid) {
        next_ex_mem.valid = true;
        next_ex_mem.inst = hmt_ex_.inst;
        next_ex_mem.fatptr = hmt_ex_.fatptr;
        switch (hmt_ex_.inst.opcode) {
            case SimdOpcode::Add128:
                {
                    auto lhs = resolve_vec_operand(hmt_ex_.inst.rs1);
                    auto rhs = resolve_vec_operand(hmt_ex_.inst.rs2);
                    for (size_t i = 0; i < 4; ++i) {
                        next_ex_mem.vec_result[i] = lhs[i] + rhs[i];
                    }
                }
                break;
            case SimdOpcode::Ld128:
                break;
            case SimdOpcode::St128:
                next_ex_mem.vec_operand = resolve_vec_operand(hmt_ex_.inst.rs1);
                break;
            case SimdOpcode::EqualExit:
                next_ex_mem.vec_operand = resolve_vec_operand(hmt_ex_.inst.rs1);
                break;
            case SimdOpcode::Jump:
                if (hmt_ex_.inst.imm < 0 ||
                    static_cast<size_t>(hmt_ex_.inst.imm) >= program_.size()) {
                    throw std::out_of_range("jump target out of range");
                }
                next_ex_mem.jump_taken = true;
                next_ex_mem.jump_target = static_cast<size_t>(hmt_ex_.inst.imm);
                break;
            case SimdOpcode::FatptrLi:
                next_ex_mem.fatptr_result = hmt_ex_.inst.fatptr_imm;
                break;
            case SimdOpcode::FatptrAdd4:
                next_ex_mem.fatptr_result = hmt_ex_.fatptr;
                next_ex_mem.fatptr_result.offset += 4;
                break;
            case SimdOpcode::FatptrSub4:
                next_ex_mem.fatptr_result = hmt_ex_.fatptr;
                next_ex_mem.fatptr_result.offset -= 4;
                break;
            case SimdOpcode::Nop:
                break;
        }
    }

    HmtEx next_hmt_ex{};
    if (id_hmt_.valid) {
        next_hmt_ex.valid = true;
        next_hmt_ex.inst = id_hmt_.inst;
        if (uses_fatptr(id_hmt_.inst)) {
            if (id_hmt_.inst.opcode == SimdOpcode::FatptrLi) {
                next_hmt_ex.fatptr = id_hmt_.inst.fatptr_imm;
            } else {
                validate_reg_index(id_hmt_.inst.frs1, kFatptrRegisters, "fatptr register out of range");
                next_hmt_ex.fatptr = fregs_[id_hmt_.inst.frs1];
            }
        }

        if (id_hmt_.inst.opcode == SimdOpcode::Ld128 ||
            id_hmt_.inst.opcode == SimdOpcode::St128 ||
            id_hmt_.inst.opcode == SimdOpcode::EqualExit) {
            if (!memory_->check_fatptr(next_hmt_ex.fatptr, kVectorBytes)) {
                throw std::out_of_range("invalid fatptr in HMT stage");
            }
        }
    }

    IdHmt next_id_hmt{};
    if (if_id_.valid) {
        next_id_hmt.valid = true;
        next_id_hmt.inst = if_id_.inst;
    }

    IfId next_if_id{};
    if (!cpu_stop_) {
        if (pc_ < program_.size()) {
            next_if_id.valid = true;
            next_if_id.pc = pc_;
            next_if_id.inst = program_[pc_];
        } else {
            cpu_stop_ = true;
        }
    }

    size_t next_pc = pc_;
    if (ex_mem_.valid && ex_mem_.jump_taken) {
        next_pc = ex_mem_.jump_target;
        next_if_id.valid = false;
    } else if (!cpu_stop_) {
        next_pc = pc_ + 1;
    }

    if_id_ = next_if_id;
    id_hmt_ = next_id_hmt;
    hmt_ex_ = next_hmt_ex;
    ex_mem_ = next_ex_mem;
    mem_wb_ = next_mem_wb;
    pc_ = next_pc;
}

void SimdCpu::run(size_t max_cycles) {
    for (size_t cycle = 0; cycle < max_cycles; ++cycle) {
        tick();
        if (cpu_stop_ && !if_id_.valid && !id_hmt_.valid && !hmt_ex_.valid && !ex_mem_.valid && !mem_wb_.valid) {
            break;
        }
    }
}
