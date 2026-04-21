#include "HMT.h"
#include "PESim.h"
#include "cpu.h"
#include "dramsim3_wrapper.h"
#include "memory_system.h"
#include "pesim-configs.h"
#include <cassert>
#include <memory>
#include <queue>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <algorithm>


SimdSim::SimdSim() : memory_(0, 0, 0, 0), cpu_(&memory_){
    MEMsim_dramsim3 = std::make_unique<dramsim3_wrapper>(0, 0, 0, 0, false);
}

bool SimdSim::empty_program(){
    return program_.empty();
}


void SimdSim::run_MEM(sim_option_t sim){
    if(sim.batch_sz == 0) {
        run_MEM_nobatch(sim);
    } else {
        run_MEM_batch(sim);
    }
    return;
}


void SimdSim::run_MEM_adaptive(sim_option_t opt) {
    size_t i = 0;
    bool early_stop = false;
    bool keep_issue = false;

    int batch_limit = opt.batch_sz;
    int issued_in_batch = 0;
    int sync_overhead = 0;
    for(; i < opt.max_cycle; i++) {
        if (traces_.empty() && MEMsim_dramsim3->drained()) {
            early_stop = true;
            break;
        }
        if (issued_in_batch >= batch_limit) {
            sync_overhead++;
            if (MEMsim_dramsim3->drained()) {
                issued_in_batch = 0;
                std::cout<<"Sync overhead for this time is: "<<sync_overhead<<std::endl;
                sync_overhead = 0;
            }
        }

        if (!traces_.empty() && ((issued_in_batch < batch_limit) || keep_issue) ) {
            trace_ent_t tr = traces_.top();
            bool is_write = (tr.op == WRITE);

            if (i >= tr.time) {
                if (MEMsim_dramsim3->WillAcceptTransaction(tr.addr, is_write)) {
                    MEMsim_dramsim3->AddTransaction(tr.addr, is_write, false);
                    issued_in_batch++;
                    traces_.pop();

                    bool next_is_write = (traces_.top().op == WRITE);
                    if(!(next_is_write ^ is_write)) keep_issue = true;
                    else keep_issue = false;
                }
            }
        }

        MEMsim_dramsim3->ClockTick();
    }

    if(early_stop){
        std::cout << "MEM Simulation finished before time runs out :)" << std::endl;
    }
    std::cout << "[RESULT]MEM Simulation done in cycle: " << i << std::endl;
}

void SimdSim::run_MEM_batch(sim_option_t opt){
    size_t i = 0;
    bool early_stop = false;

    int batch_limit = opt.batch_sz;
    int issued_in_batch = 0;
    int sync_overhead = 0;
    for(; i < opt.max_cycle; i++) {
        if (traces_.empty() && MEMsim_dramsim3->drained()) {
            early_stop = true;
            break;
        }
        if (issued_in_batch >= batch_limit) {
            sync_overhead++;
            if (MEMsim_dramsim3->drained()) {
                issued_in_batch = 0;
                std::cout<<"Sync overhead for this time is: "<<sync_overhead<<std::endl;
                sync_overhead = 0;
            }
        }

        if (!traces_.empty() && issued_in_batch < batch_limit) {
            trace_ent_t tr = traces_.top();
            bool is_write = (tr.op == WRITE);

            if (i >= tr.time) {
                if (MEMsim_dramsim3->WillAcceptTransaction(tr.addr, is_write)) {
                    MEMsim_dramsim3->AddTransaction(tr.addr, is_write, false);
                    issued_in_batch++;
                    traces_.pop();
                }
            }
        }

        MEMsim_dramsim3->ClockTick();
    }

    if(early_stop){
        std::cout << "MEM Simulation finished before time runs out :)" << std::endl;
    }
    std::cout << "[RESULT]MEM Simulation done in cycle: " << i << std::endl;
}

void SimdSim::run_MEM_nobatch(sim_option_t opt){
    size_t i = 0;
    bool early_stop = false;
    for(;i < opt.max_cycle && !traces_.empty(); i++) {
        trace_ent_t tr = traces_.top();
        bool is_write = (tr.op == WRITE);
        if(i >= tr.time) {
            if(MEMsim_dramsim3->WillAcceptTransaction(tr.addr, is_write)){
                MEMsim_dramsim3->AddTransaction(tr.addr, is_write, false);
                traces_.pop();
            }
        }

        if(MEMsim_dramsim3->drained() && traces_.empty()){
            early_stop = true;
            break;
        }
        MEMsim_dramsim3->ClockTick();
    }
    if(early_stop || traces_.empty()){
        std::cout<<"MEM Simulation finished before time runs out :)"<<std::endl;
    }
    std::cout<<"[RESULT]MEM Simulation done in cycle: "<<i<<std::endl;
}
void SimdSim::run_PIM(sim_option_t opt){
    size_t i = 0;
    bool early_stop = false;
    for(;i < opt.max_cycle; i++) {
        cpu_.tick();
        cpu_.inc_cycl();

        if(cpu_.is_stopped()){
            early_stop = true;
            break;
        }
    }

    if(early_stop){
        std::cout<<"PIM Simulation finished before time runs out :)"<<std::endl;
    }

    std::cout<<"[RESULT]PIM Simulation done in cycle: "<<i<<std::endl;

}


void SimdSim::run_GENADDR(sim_option_t opt){
    gen_trace::TraceType acc_typ = gen_trace::TraceType::UNDEF;

    if(opt.gen_trace_typ == 1) {
        acc_typ = gen_trace::TraceType::STREAM;
    } else if(opt.gen_trace_typ == 2) {
        acc_typ = gen_trace::TraceType::RANDOM;
    } else if(opt.gen_trace_typ == 3) {
        acc_typ = gen_trace::TraceType::MIX;
    } else {
        std::cerr<<"Wrong trace type: "<<acc_typ<<", Simulation stopeed :("<<std::endl;
        exit(-1);
    }
    gen_trace gen(acc_typ, opt.num, opt.t_interval, opt.rw_ratio, opt.all_read, opt.all_write, memory_.get_dsim_wrapper());
    gen.generate();

    return;
}

void SimdSim::run(sim_option_t opt) {
    switch(opt.mode){
        case sim_mode::MEM:
            run_MEM(opt);
            break;
        case sim_mode::PIM:
            run_PIM(opt);
            break;
        case sim_mode::HYBRID:
            run_HYBRID(opt);
            break;
        case sim_mode::GEN_ADDR:
            run_GENADDR(opt);
            break;
        case sim_mode::Adaptive_HYBRID:
            run_Adaptive_HYBRID(opt);
            break;
    }

    return;
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
