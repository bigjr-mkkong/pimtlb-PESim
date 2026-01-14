#include <cassert>
#include <stdexcept>
#include "cpu.h"
#include "HMT.h"

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
        case SimdOpcode::FatptrAdd:
        case SimdOpcode::FatptrSub:
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
    auto is_mem_op = [](SimdOpcode opc) {
        return opc == SimdOpcode::Ld128 || opc == SimdOpcode::St128 || opc == SimdOpcode::EqualExit;
    };

    bool mem_stall = false;
    if (ex_mem_.valid && is_mem_op(ex_mem_.inst.opcode) && ex_mem_.mem_delay_remaining > 0) {
        ex_mem_.mem_delay_remaining -= 1;
        mem_stall = true;
    }

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
            case SimdOpcode::FatptrAdd:
            case SimdOpcode::FatptrSub:
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
    if (!mem_stall && ex_mem_.valid) {
        next_mem_wb.valid = true;
        next_mem_wb.inst = ex_mem_.inst;
        switch (ex_mem_.inst.opcode) {
            case SimdOpcode::Ld128:
                next_mem_wb.vec_result = memory_->load128(ex_mem_.phys_addr);
                break;
            case SimdOpcode::St128:
                memory_->store128(ex_mem_.phys_addr, ex_mem_.vec_operand);
                break;
            case SimdOpcode::Add128:
                next_mem_wb.vec_result = ex_mem_.vec_result;
                break;
            case SimdOpcode::EqualExit:
                next_mem_wb.should_stop = memory_->equal128(ex_mem_.phys_addr, ex_mem_.vec_operand);
                break;
            case SimdOpcode::FatptrLi:
            case SimdOpcode::FatptrAdd:
            case SimdOpcode::FatptrSub:
                next_mem_wb.fatptr_result = ex_mem_.fatptr_result;
                break;
            case SimdOpcode::Jump:
            case SimdOpcode::Nop:
                break;
        }
    }

    ExMem next_ex_mem{};
    if (mem_stall) {
        next_ex_mem = ex_mem_;
    } else if (hmt_ex_.valid) {
        next_ex_mem.valid = true;
        next_ex_mem.inst = hmt_ex_.inst;
        next_ex_mem.fatptr = hmt_ex_.fatptr;
        next_ex_mem.phys_addr = hmt_ex_.phys_addr;
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
                next_ex_mem.mem_delay_remaining = memory_->get_delay_cycl(next_ex_mem.phys_addr);
                break;
            case SimdOpcode::St128:
                next_ex_mem.vec_operand = resolve_vec_operand(hmt_ex_.inst.rs1);
                next_ex_mem.mem_delay_remaining = memory_->get_delay_cycl(next_ex_mem.phys_addr);
                break;
            case SimdOpcode::EqualExit:
                next_ex_mem.vec_operand = resolve_vec_operand(hmt_ex_.inst.rs1);
                next_ex_mem.mem_delay_remaining = memory_->get_delay_cycl(next_ex_mem.phys_addr);
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
            case SimdOpcode::FatptrAdd:
                {
                    auto lane = resolve_vec_operand(hmt_ex_.inst.rs1);
                    int index = hmt_ex_.inst.mask & 0x3;
                    next_ex_mem.fatptr_result = hmt_ex_.fatptr;
                    next_ex_mem.fatptr_result.offset += static_cast<int32_t>(lane[index]);
                }
                break;
            case SimdOpcode::FatptrSub:
                {
                    auto lane = resolve_vec_operand(hmt_ex_.inst.rs1);
                    int index = hmt_ex_.inst.mask & 0x3;
                    next_ex_mem.fatptr_result = hmt_ex_.fatptr;
                    next_ex_mem.fatptr_result.offset -= static_cast<int32_t>(lane[index]);
                }
                break;
            case SimdOpcode::Nop:
                break;
        }
    }

    HmtEx next_hmt_ex{};
    if (mem_stall) {
        next_hmt_ex = hmt_ex_;
    } else if (id_hmt_.valid) {
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
            next_hmt_ex.phys_addr = memory_->translate_fatptr(next_hmt_ex.fatptr, kVectorBytes);
        }
    }

    IdHmt next_id_hmt{};
    if (mem_stall) {
        next_id_hmt = id_hmt_;
    } else if (if_id_.valid) {
        next_id_hmt.valid = true;
        next_id_hmt.inst = if_id_.inst;
    }

    IfId next_if_id{};
    if (mem_stall) {
        next_if_id = if_id_;
    } else if (!cpu_stop_) {
        if (pc_ < program_.size()) {
            next_if_id.valid = true;
            next_if_id.pc = pc_;
            next_if_id.inst = program_[pc_];
        } else {
            cpu_stop_ = true;
        }
    }

    size_t next_pc = pc_;
    if (mem_stall) {
        next_pc = pc_;
    } else if (ex_mem_.valid && ex_mem_.jump_taken) {
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
        case SimdOpcode::FatptrAdd:
        case SimdOpcode::FatptrSub:
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
            case SimdOpcode::FatptrAdd:
            case SimdOpcode::FatptrSub:
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
            case SimdOpcode::FatptrAdd:
            case SimdOpcode::FatptrSub:
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
            case SimdOpcode::FatptrAdd:
                {
                    auto lane = resolve_vec_operand(hmt_ex_.inst.rs1);
                    int index = hmt_ex_.inst.mask & 0x3;
                    next_ex_mem.fatptr_result = hmt_ex_.fatptr;
                    next_ex_mem.fatptr_result.offset += static_cast<int32_t>(lane[index]);
                }
                break;
            case SimdOpcode::FatptrSub:
                {
                    auto lane = resolve_vec_operand(hmt_ex_.inst.rs1);
                    int index = hmt_ex_.inst.mask & 0x3;
                    next_ex_mem.fatptr_result = hmt_ex_.fatptr;
                    next_ex_mem.fatptr_result.offset -= static_cast<int32_t>(lane[index]);
                }
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
