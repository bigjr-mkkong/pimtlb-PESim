#include "HMT.h"
#include "PESim.h"
#include "cpu.h"
#include "dramsim3_wrapper.h"
#include "memory_system.h"
#include "pesim-configs.h"
#include <cassert>
#include <memory>
#include <ostream>
#include <queue>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <algorithm>

void SimdSim::run_HYBRID(sim_option_t opt){
    auto peek_next_op = [](std::vector<trace_ent_t> traces, size_t time) {
        for(auto &it: traces) {
            if(it.time > time) return it.op;
        }
        return ext_op_t::UNDEF;
    };

    int MEM_BATCH_SZ = opt.batch_sz;
    int MAX_MC_QUEUE_SZ = 64; // Physical hardware queue limit
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

    // Dedicated clock for memory trace processing
    size_t mem_clock = 0;

    struct PendingReq {
        uint64_t addr;
        bool is_write;
    };
    std::vector<PendingReq> pending_reqs;

    for (size_t i = 0; i < opt.max_cycle; ++i) {

        // ---------------------------------------------------------
        // 1. HARDWARE MECHANISMS: Clocks and Physical Tracking
        // ---------------------------------------------------------

        // mem_clock ONLY advances when the memory controller is active
        bool is_mem_state = (sim_mode == MEM_WAIT_BATCH || sim_mode == MEM_RUN);
        if (is_mem_state) {
            MEMsim_dramsim3->ClockTick();
            mem_clock++;
        }

        bool pe_stopped = cpu_.is_stopped();
        bool pe_ready = pe_stopped ? true : cpu_.ready4signal();

        // Check DRAM Progress (Update drained status)
        bool reqs_drained = true;
        for (auto it = pending_reqs.begin(); it != pending_reqs.end(); ) {
            int pending_count = it->is_write ? MEMsim_dramsim3->get_pend_write(it->addr, false)
                                             : MEMsim_dramsim3->get_pend_read(it->addr, false);
            if (pending_count == 0) {
                it = pending_reqs.erase(it);
            } else {
                reqs_drained = false;
                ++it;
            }
        }

        // Issue Logic
        bool can_issue_global = true;
        if (sim_mode == MEM_RUN) {
            can_issue_global = (batch_left > 0 || !pe_ready || pe_stopped);
        }

        if (can_issue_global && !traces_.empty() && pending_reqs.size() < MAX_MC_QUEUE_SZ) {
            trace_ent_t tr = traces_.top();

            // [MODIFIED]: Strict Check against the paused mem_clock.
            // Traces will NOT arrive in the background while PIM is running.
            if (tr.time <= mem_clock) {
                bool is_write = (tr.op != READ);
                bool read_blocked_by_write = (!is_write && MEMsim_dramsim3->get_pend_write(tr.addr, false) != 0);
                if (!read_blocked_by_write && MEMsim_dramsim3->WillAcceptTransaction(tr.addr, is_write)) {
                    MEMsim_dramsim3->AddTransaction(tr.addr, is_write, false);
                    pending_reqs.push_back({tr.addr, is_write});
                    traces_.pop();

                    if (sim_mode == MEM_WAIT_BATCH || sim_mode == MEM_RUN) {
                        batch_left--;
                    }
                }
            }
        }

        // ---------------------------------------------------------
        // 2. SCHEDULING POLICY: The Decoupled Decision Maker
        // ---------------------------------------------------------

        auto scheduling_policy = [&]() -> SimMode {
            switch (sim_mode) {
                case MEM_WAIT_BATCH:
                    if (!pending_reqs.empty()) return MEM_RUN;
                    if (traces_.empty() && !pe_stopped) return SWITCH_TO_PIM;
                    return MEM_WAIT_BATCH;

                case MEM_RUN: {
                    // [MODIFIED]: Check trace arrival against mem_clock to prevent premature switching
                    bool active_traces = !traces_.empty() && traces_.top().time <= mem_clock;
                    bool want_to_issue = can_issue_global && active_traces;

                    if (!pe_stopped && batch_started && pe_ready && reqs_drained && !want_to_issue) {
                        return SWITCH_TO_PIM;
                    }
                    return MEM_RUN;
                }

                case SWITCH_TO_PIM:
                    if (pe_stopped) return SWITCH_TO_MEM;
                    if (!pimcpu_started || cpu_.ready4signal()) return PIM_RUN;
                    return SWITCH_TO_PIM;

                case PIM_RUN:
                    if (pe_stopped || cpu_.is_stopped() || (!traces_.empty() && pim_budget_cycles >= mem_budget_cycles)) {
                        return SWITCH_TO_MEM;
                    }
                    return PIM_RUN;

                case SWITCH_TO_MEM:
                    if (pe_stopped || cpu_.ready4signal()) return MEM_WAIT_BATCH;
                    return SWITCH_TO_MEM;

                default: return sim_mode;
            }
        };

        // ---------------------------------------------------------
        // 3. STATE MACHINE EXECUTION: Processing the Policy
        // ---------------------------------------------------------

        SimMode next_mode = scheduling_policy();

        if (next_mode != sim_mode) {
            if (next_mode == MEM_RUN) {
                batch_started = true;
                mem_budget_cycles = 0;
            } else if (next_mode == SWITCH_TO_PIM) {
                pending_reqs.clear();
                pim_budget_cycles = 0;
                if (pimcpu_started) cpu_.resume();
            } else if (next_mode == PIM_RUN) {
                if (!pimcpu_started) pimcpu_started = true;
            } else if (next_mode == SWITCH_TO_MEM) {
                if (!cpu_.is_stopped() && sim_mode == PIM_RUN) cpu_.pause();
            } else if (next_mode == MEM_WAIT_BATCH) {
                batch_left = MEM_BATCH_SZ;
                batch_started = false;
                mem_budget_cycles = 0;
                pim_budget_cycles = 0;
            }
            sim_mode = next_mode;
        }

        if (sim_mode == MEM_RUN) {
            mem_budget_cycles++;
        } else if (sim_mode == SWITCH_TO_PIM) {
            if (pimcpu_started) {
                cpu_.tick();
                cpu_.inc_cycl();
            }
        } else if (sim_mode == PIM_RUN) {
            cpu_.tick();
            cpu_.inc_cycl();
            pim_budget_cycles++;
        } else if (sim_mode == SWITCH_TO_MEM) {
            if (!pe_stopped) {
                cpu_.tick();
                cpu_.inc_cycl();
            }
        }

        // ---------------------------------------------------------
        // 4. END CONDITIONS
        // ---------------------------------------------------------

        if (pe_stopped && !pe_fin) pe_fin = true;
        if (traces_.empty() && !trace_fin && MEMsim_dramsim3->drained()) trace_fin = true;
        if (pe_fin && trace_fin) {
            std::cout << "[RESULT]Corrected HYBRID Simulation done. Total Cycles: " << i << std::endl;
            break;
        }
        cycl = i;
    }
}


void SimdSim::run_Adaptive_HYBRID(sim_option_t opt){
    // int MEM_BATCH_SZ = opt.batch_sz;
    // size_t cycl = 0;
    // cpu_.load_program(program_);

    // bool pe_fin = false, trace_fin = false;
    // if(traces_.empty()){
    //    std::cout<<"Trace is empty, this simulation will run without stop"<<std::endl;
    // }

    // enum SimMode {
    //     MEM_WAIT_BATCH,
    //     MEM_RUN,
    //     SWITCH_TO_PIM,
    //     PIM_RUN,
    //     SWITCH_TO_MEM
    // };

    // bool pimcpu_started = false;
    // SimMode sim_mode = MEM_WAIT_BATCH;

    // int mem_budget_cycles = 0;
    // int pim_budget_cycles = 0;
    // int batch_left = MEM_BATCH_SZ;
    // bool batch_started = false;

    // // Track the operation type of the current batch to know when to extend
    // ext_op_t last_issued_op = ext_op_t::UNDEF;

    // // Dedicated clock for memory trace arrival to prevent PIM cycles
    // // from "hiding" inside the memory timeline.
    // size_t mem_clock = 0;

    // // Define a struct to track both reads and writes uniformly
    // struct PendingReq {
    //     uint64_t addr;
    //     bool is_write;
    // };
    // std::vector<PendingReq> pending_reqs;

    // for (size_t i = 0; i < opt.max_cycle; ++i) {

    //     // 1. SERIAL CLOCK LOGIC:
    //     // Tick DRAM and advance the trace arrival clock ONLY in MEM states.
    //     bool is_mem_state = (sim_mode == MEM_WAIT_BATCH || sim_mode == MEM_RUN);
    //     if (is_mem_state) {
    //         MEMsim_dramsim3->ClockTick();
    //         mem_clock++;
    //     }

    //     // Cache the stopped state at the start of the cycle
    //     bool pe_stopped = cpu_.is_stopped();

    //     switch (sim_mode) {

    //     case MEM_WAIT_BATCH: {
    //         if (!traces_.empty()) {
    //             trace_ent_t tr = traces_.top();

    //             // FIX: Check arrival against mem_clock, not global loop counter i
    //             if (tr.time <= mem_clock) {
    //                 bool is_write = (tr.op != READ);
    //                 bool read_blocked_by_write = (!is_write && MEMsim_dramsim3->get_pend_write(tr.addr, false) != 0);

    //                 if (!read_blocked_by_write && MEMsim_dramsim3->WillAcceptTransaction(tr.addr, is_write)) {
    //                     MEMsim_dramsim3->AddTransaction(tr.addr, is_write, false);

    //                     pending_reqs.push_back({tr.addr, is_write});
    //                     last_issued_op = tr.op; // Initialize the batch's operation type
    //                     traces_.pop();

    //                     batch_left = MEM_BATCH_SZ - 1;
    //                     batch_started = true;
    //                     mem_budget_cycles = 0;
    //                     sim_mode = MEM_RUN;
    //                 }
    //             }
    //         } else if (!pe_stopped) {
    //             sim_mode = SWITCH_TO_PIM;
    //             pim_budget_cycles = 0;
    //             if (pimcpu_started) {
    //                 std::cout<<"PIMCPU trying to resume @ "<< i << std::endl;
    //                 cpu_.resume();
    //             }
    //         }
    //         break;
    //     }

    //     case MEM_RUN: {
    //         bool pe_ready = pe_stopped ? true : cpu_.ready4signal();

    //         ext_op_t next_op = ext_op_t::UNDEF;
    //         size_t next_time = 0;
    //         if (!traces_.empty()) {
    //             next_op = traces_.top().op;
    //             next_time = traces_.top().time;
    //         }

    //         bool adaptive_extend = (batch_left <= 0) && (next_op == last_issued_op) && (next_op != ext_op_t::UNDEF);

    //         bool can_issue = (batch_left > 0 || adaptive_extend || !pe_ready || pe_stopped);

    //         bool reqs_drained = true;
    //         if (can_issue && !traces_.empty()) {
    //             trace_ent_t tr = traces_.top();

    //             // FIX: Check arrival against mem_clock
    //             if (tr.time <= mem_clock) {
    //                 bool is_write = (tr.op != READ);
    //                 bool read_blocked_by_write = (!is_write && MEMsim_dramsim3->get_pend_write(tr.addr, false) != 0);

    //                 if (!read_blocked_by_write && MEMsim_dramsim3->WillAcceptTransaction(tr.addr, is_write)) {
    //                     MEMsim_dramsim3->AddTransaction(tr.addr, is_write, false);

    //                     pending_reqs.push_back({tr.addr, is_write});
    //                     last_issued_op = tr.op; // Update last issued op (redundant if extending, but safe)
    //                     traces_.pop();

    //                     if (batch_left > 0) {
    //                         batch_left--; // Only decrement if we are still within the lower bound
    //                     }
    //                 }
    //             }
    //         }

    //         mem_budget_cycles++;

    //         for (auto it = pending_reqs.begin(); it != pending_reqs.end(); ) {
    //             int pending_count = it->is_write ? MEMsim_dramsim3->get_pend_write(it->addr, false)
    //                                              : MEMsim_dramsim3->get_pend_read(it->addr, false);
    //             if (pending_count == 0) {
    //                 it = pending_reqs.erase(it);
    //             } else {
    //                 reqs_drained = false;
    //                 ++it;
    //             }
    //         }

    //         // FIX: Check active traces against mem_clock
    //         bool active_traces = !traces_.empty() && traces_.top().time <= mem_clock;
    //         bool want_to_issue = can_issue && active_traces;

    //         const size_t IDLE_WAIT_WINDOW = 20;

    //         bool future_match_arriving_soon = false;
    //         if (!traces_.empty() && can_issue) {
    //             // FIX: Check future arrival against mem_clock instead of i!
    //             if (next_op == last_issued_op && next_time <= (mem_clock + IDLE_WAIT_WINDOW)) {
    //                 future_match_arriving_soon = true;
    //             }
    //         }

    //         // We only yield to PIM if we don't want to issue NOW, AND we don't want to WAIT.
    //         bool want_to_stay = want_to_issue || future_match_arriving_soon;

    //         if (!pe_stopped && batch_started && pe_ready && reqs_drained && !want_to_stay) {
    //             pending_reqs.clear();
    //             pim_budget_cycles = 0;
    //             sim_mode = SWITCH_TO_PIM;

    //             if (pimcpu_started) {
    //                 std::cout<<"PIMCPU trying to resume @ "<< i << std::endl;
    //                 cpu_.resume();
    //             }
    //         }
    //         break;
    //     }

    //     case SWITCH_TO_PIM: {
    //         if (pe_stopped) {
    //             sim_mode = SWITCH_TO_MEM;
    //             break;
    //         }

    //         if (!pimcpu_started) {
    //             pimcpu_started = true;
    //             sim_mode = PIM_RUN;
    //         } else {
    //             cpu_.tick(); // Switching overhead
    //             cpu_.inc_cycl();

    //             if (cpu_.ready4signal()) {
    //                 sim_mode = PIM_RUN;
    //             }
    //         }
    //         break;
    //     }

    //     case PIM_RUN: {
    //         if (pe_stopped) {
    //             sim_mode = SWITCH_TO_MEM;
    //             break;
    //         }

    //         cpu_.tick();
    //         cpu_.inc_cycl();
    //         pimcpu_started = true;
    //         pim_budget_cycles++;

    //         if (cpu_.is_stopped() || (!traces_.empty() && pim_budget_cycles >= mem_budget_cycles)) {
    //             if (!cpu_.is_stopped()) {
    //                 std::cout<<"PIMCPU trying to pause @ "<< i<<std::endl;
    //                 cpu_.pause();
    //             }
    //             sim_mode = SWITCH_TO_MEM;
    //         }
    //         break;
    //     }

    //     case SWITCH_TO_MEM: {
    //         if (!pe_stopped) {
    //             cpu_.tick(); // Switching overhead
    //             cpu_.inc_cycl();
    //         }

    //         if (pe_stopped || cpu_.ready4signal()) {
    //             batch_left = MEM_BATCH_SZ;
    //             batch_started = false;
    //             mem_budget_cycles = 0;
    //             pim_budget_cycles = 0;
    //             sim_mode = MEM_WAIT_BATCH;
    //         }
    //         break;
    //     }

    //     default:
    //         break;
    //     }

    //     // --- Termination Logic ---
    //     if (pe_stopped && !pe_fin) {
    //         std::cout << "Program finished at: " << i << std::endl;
    //         pe_fin = true;
    //     }

    //     if (traces_.empty() && !trace_fin && MEMsim_dramsim3->drained()) {
    //         std::cout << "Trace finished at: " << i << std::endl;
    //         trace_fin = true;
    //     }

    //     if (pe_fin && trace_fin) {
    //         std::cout << "[RESULT]Adaptive HYBRID Simulation done :) Cycle: " << i << std::endl;
    //         break;
    //     }

    //     // Update cycl tracker for the end failure print block
    //     cycl = i;
    // }

    // if(cycl == opt.max_cycle - 1){
    //     std::cout<<"Adaptive HYBRID Simulation finished before workload runs out, did you give it enough time?"<<std::endl;
    // }
}
