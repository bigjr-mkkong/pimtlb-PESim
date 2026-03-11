#include "PESim.h"
#include "memory_system.h"
#include <cassert>
#include <memory>
#include <queue>
#include <cstdio>
#include <cstdlib>
#include <iostream>

#define MEM_BATCH_SZ    1

void SimdSim::dramsim3_read_callback(uint64_t addr) {
    pendmap[addr] -= 1;
    if(pendmap[addr] < 0){
        std::cerr<<"dramsim3 read callback failed"<<std::endl;
        exit(1);
    }
    return;
}

void SimdSim::dramsim3_write_callback(uint64_t addr) {
    pendmap[addr] -= 1;
    if(pendmap[addr] < 0){
        std::cerr<<"dramsim3 write callback failed"<<std::endl;
        exit(1);
    }
    return;
}

bool SimdSim::dramsim3_empty(){
    for(auto &i: pendmap) {
        if(i.second > 0)
            return false;
    }

    return true;
}


SimdSim::SimdSim() : cpu_(&memory_) {
    program_.push_back(SimdInstruction{
        .opcode = SimdOpcode::Add128,
        .rd = 1,
        .rs1 = 1,
        .rs2 = 2,
        .imm = 0,
        .mask = 0,
        .fatptr_imm = {0, 0},
    });
    program_.push_back(SimdInstruction{
        .opcode = SimdOpcode::Jump,
        .rd = 0,
        .rs1 = 0,
        .rs2 = 0,
        .imm = 0,
        .mask = 0,
        .fatptr_imm = {0, 0},
    });
    
    dramsim3 = std::make_unique<dramsim3::MemorySystem>(
        "/home/michael/Projects/pimtlb/PIM-AutoDSE/libpimeval/dramsim3/configs",
        "/home/michael/Projects/pimtlb/PIM-AutoDSE/libpimeval/output",
        [this](uint64_t addr) {this->dramsim3_read_callback(addr);},
        [this](uint64_t addr) {this->dramsim3_write_callback(addr);}
    );
}

bool SimdSim::empty_program(){
    return program_.empty();
}

void SimdSim::run(size_t max_cycles) {
    size_t cycl = 0;
    cpu_.load_program(program_);
    // cpu_.run(max_cycles);

    bool pe_fin = false, trace_fin = false;
    if(traces_.empty()){
       std::cout<<"Trace is empty, this simulation will run without stop"<<std::endl;
    }

    enum {
        MEM,
        PIM
    } sim_mode;


    bool pimcpu_started = false;
    sim_mode = MEM;

    int tMEM = 0, tPIM = 0;
    int batch_size = MEM_BATCH_SZ;

    for (size_t i = 0; i < max_cycles; ++i) {
        if(sim_mode == MEM && !traces_.empty()) {
            trace_ent_t tr = traces_.top();
            if(tr.time <= i) {
                if(tr.op == READ) {
                    if(dramsim3->WillAcceptTransaction(tr.addr, false)){
                        dramsim3->AddTransaction(tr.addr, false);
                    }
                } else {
                    if(dramsim3->WillAcceptTransaction(tr.addr, true)){
                        dramsim3->AddTransaction(tr.addr, true);
                    }
                }
                batch_size--;
            }
        }

        if(sim_mode == MEM) {
            dramsim3->ClockTick();
            tMEM++;
            if(dramsim3_empty()) {
                sim_mode = PIM;

                if(pimcpu_started)
                    cpu_.resume();
            }
        } else {
            cpu_.tick();
            cpu_.inc_cycl();
            pimcpu_started = true;
            tPIM++;
            if(tPIM == tMEM) {
                cpu_.pause();
                tPIM = 0;
                tMEM = 0;
                batch_size = MEM_BATCH_SZ;
                sim_mode = MEM;
            }
        }

        if (cpu_.is_stopped() && !pe_fin) {
            std::cout<<"Program finished at: "<<i<<std::endl;
            pe_fin = true;
        }

        if(traces_.size() == 0 && !trace_fin){
            std::cout<<"Trace finished at: "<<i<<std::endl;
            trace_fin = true;
        }

        if(pe_fin && trace_fin) {
            std::cout<<"Simulation done in cycl: "<<i<<std::endl;
            break;
        }

    }

    if(cycl == max_cycles - 1){
        std::cout<<"Simulation finished before program finished, did you give it enough time?"<<std::endl;
    }
}

SimdCpu &SimdSim::cpu() {
    return cpu_;
}

const SimdCpu &SimdSim::cpu() const {
    return cpu_;
}

SimdMemory &SimdSim::memory() {
    return memory_;
}

const SimdMemory &SimdSim::memory() const {
    return memory_;
}

void SimdSim::add_region(uint32_t varidx, size_t size_bytes) {
    memory_.add_region(varidx, size_bytes);
}

void SimdSim::fill_region(uint32_t varidx, const std::vector<uint8_t> &src){
    memory_.fill_region(varidx, src);
}

void SimdSim::set_vreg(size_t idx, const std::array<uint32_t, 4> &value){
    cpu_.set_vreg(idx, value);
}

void SimdSim::load_program(const std::vector<SimdInstruction> &program) {
    program_ = program;
    cpu_.load_program(program_);
}

void SimdSim::load_trace(const std::priority_queue<trace_ent_t> &traces) {
    traces_ = traces;
    cpu_.load_trace(traces_);
}
