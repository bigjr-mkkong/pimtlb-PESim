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


SimdSim::SimdSim() : memory_(0, 0, 0, 0), cpu_(&memory_){
    MEMsim_dramsim3 = std::make_unique<dramsim3_wrapper>(0, 0, 0, 0, false);
}

bool SimdSim::empty_program(){
    return program_.empty();
}

void SimdSim::run_MEM(sim_option_t opt){
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
    std::cout<<"MEM Simulation done in cycle: "<<i<<std::endl;
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

    std::cout<<"PIM Simulation done in cycle: "<<i<<std::endl;

}
#define MEM_BATCH_SZ    4

#include <vector>
#include <iostream>

void SimdSim::run_HYBRID(sim_option_t opt){
    size_t cycl = 0;
    cpu_.load_program(program_);

    bool pe_fin = false, trace_fin = false;
    if(traces_.empty()){
       std::cout<<"Trace is empty, this simulation will run without stop"<<std::endl;
    }

    enum SimMode {
        MEM_WAIT_BATCH,
        MEM_RUN,
        SWITCH_TO_PIM,
        PIM_RUN,
        SWITCH_TO_MEM
    };

    bool pimcpu_started = false;

    SimMode sim_mode = MEM_WAIT_BATCH;

    int mem_budget_cycles = 0;
    int pim_budget_cycles = 0;
    int batch_left = MEM_BATCH_SZ;

    bool batch_started = false;

    // Tracks pending read addresses to know when we are safe to switch
    // without waiting for deferred writes to finish.
    std::vector<uint64_t> pending_reads;

    for (size_t i = 0; i < opt.max_cycle; ++i) {

        // Cache the stopped state at the start of the cycle
        bool pe_stopped = cpu_.is_stopped();

        switch (sim_mode) {

        case MEM_WAIT_BATCH: {

            if (!traces_.empty()) {
                trace_ent_t tr = traces_.top();

                if (tr.time <= i) {
                    bool is_write = (tr.op != READ);

                    // To ensure read gets the newest value, stall if there's a pending write to the same address.
                    bool read_blocked_by_write = (!is_write && MEMsim_dramsim3->get_pend_write(tr.addr, false) != 0);

                    if (!read_blocked_by_write && MEMsim_dramsim3->WillAcceptTransaction(tr.addr, is_write)) {
                        MEMsim_dramsim3->AddTransaction(tr.addr, is_write, false);

                        if (!is_write) {
                            pending_reads.push_back(tr.addr);
                        }

                        traces_.pop();

                        batch_left = MEM_BATCH_SZ - 1;
                        batch_started = true;
                        mem_budget_cycles = 0;

                        sim_mode = MEM_RUN;
                    }
                }
            }
            break;
        }

        case MEM_RUN: {
            bool pe_ready = pe_stopped ? true : cpu_.ready4signal();

            // If the PE is stopped, bypass batch limits and consume traces as fast as DRAM allows.
            bool can_issue = (batch_left > 0 || !pe_ready || pe_stopped);

            if (can_issue && !traces_.empty()) {
                trace_ent_t tr = traces_.top();

                if (tr.time <= i) {
                    bool is_write = (tr.op != READ);

                    bool read_blocked_by_write = (!is_write && MEMsim_dramsim3->get_pend_write(tr.addr, false) != 0);

                    if (!read_blocked_by_write && MEMsim_dramsim3->WillAcceptTransaction(tr.addr, is_write)) {
                        MEMsim_dramsim3->AddTransaction(tr.addr, is_write, false);

                        if (!is_write) {
                            pending_reads.push_back(tr.addr);
                        }

                        traces_.pop();
                        batch_left--;
                    }
                }
            }

            MEMsim_dramsim3->ClockTick();
            mem_budget_cycles++;

            // Only verify if READ transactions are drained.
            bool reads_drained = true;
            for (auto it = pending_reads.begin(); it != pending_reads.end(); ) {
                if (MEMsim_dramsim3->get_pend_read(*it, false) == 0) {
                    it = pending_reads.erase(it);
                } else {
                    reads_drained = false;
                    ++it;
                }
            }

            bool active_traces = !traces_.empty() && traces_.top().time <= i;
            bool want_to_issue = can_issue && active_traces;

            // Switch back to PIM only if the PE is ready, reads are processed,
            // we have exhausted our trace demands, AND the PE is actually still running.
            if (!pe_stopped && batch_started && pe_ready && reads_drained && !want_to_issue) {
                pending_reads.clear();
                pim_budget_cycles = 0;
                sim_mode = SWITCH_TO_PIM;

                if (pimcpu_started) {
                    std::cout<<"PIMCPU trying to resume @ "<< i<<std::endl;
                    cpu_.resume();
                }
            }

            break;
        }

        case SWITCH_TO_PIM: {
            if (pe_stopped) {
                sim_mode = SWITCH_TO_MEM;
                break;
            }

            if (!pimcpu_started) {
                pimcpu_started = true;
                sim_mode = PIM_RUN;
            } else {
                cpu_.tick();
                cpu_.inc_cycl();

                if (cpu_.ready4signal()) {
                    sim_mode = PIM_RUN;
                }
            }
            break;
        }

        case PIM_RUN: {
            if (pe_stopped) {
                sim_mode = SWITCH_TO_MEM;
                break;
            }

            cpu_.tick();
            cpu_.inc_cycl();
            pimcpu_started = true;
            pim_budget_cycles++;

            if (cpu_.is_stopped() || pim_budget_cycles >= mem_budget_cycles) {
                if (!cpu_.is_stopped()) {
                    std::cout<<"PIMCPU trying to pause @ "<< i<<std::endl;
                    cpu_.pause();
                }
                sim_mode = SWITCH_TO_MEM;
            }
            break;
        }

        case SWITCH_TO_MEM: {
            if (!pe_stopped) {
                cpu_.tick();
                cpu_.inc_cycl();
            }

            // If the CPU is stopped, we skip waiting for the signal and immediately head back to wait for memory traces
            if (pe_stopped || cpu_.ready4signal()) {
                batch_left = MEM_BATCH_SZ;
                batch_started = false;
                mem_budget_cycles = 0;
                pim_budget_cycles = 0;
                sim_mode = MEM_WAIT_BATCH;
            }
            break;
        }

        default:
            break;
        }

        if (pe_stopped && !pe_fin) {
            std::cout << "Program finished at: " << i << std::endl;
            pe_fin = true;
        }

        if (traces_.empty() && !trace_fin && MEMsim_dramsim3->drained()) {
            std::cout << "Trace finished at: " << i << std::endl;
            trace_fin = true;
        }

        if (pe_fin && trace_fin) {
            std::cout << "HYBRID Simulation done :) Cycle: " << i << std::endl;
            break;
        }
    }

    if(cycl == opt.max_cycle - 1){
        std::cout<<"HYBRID Simulation finished before worload runs out, did you give it enough time?"<<std::endl;
    }
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
