#include <cstdint>
#include <iostream>
#include <memory>
#include <random>
#include "dramsim3_wrapper.h"
#include "pesim-configs.h"
#include "memory_system.h"


dramsim3_wrapper::dramsim3_wrapper(int ch, int ra, int bg, int ba, bool is_pim){
    ms = std::make_unique<dramsim3::MemorySystem>(
            MEM_CONFIG_PATH,
            MEM_OUTPUT_PATH,
            [this] (uint64_t addr) {this->read_callback(addr);},
            [this] (uint64_t addr) {this->write_callback(addr);}
    );
    ms->SetPimMode(is_pim);

    ch_ = ch;
    ra_ = ra;
    bg_ = bg;
    ba_ = ba;
}

bool dramsim3_wrapper::WillAcceptTransaction(uint64_t hex_addr, bool is_write) const {
    return ms->WillAcceptTransaction(hex_addr, is_write);
}
bool dramsim3_wrapper::AddTransaction(uint64_t hex_addr, bool is_write, bool is_pim) {
    uint64_t real_addr;
    if(is_pim) {
        real_addr = ms->BankLocalToGlobalAddr(ch_, ra_, bg_, ba_, hex_addr);
    } else {
        real_addr = hex_addr;
    }

    bool ret = ms->AddTransaction(real_addr, is_write, is_pim);

    if(ret){
        if(is_write)
            vis[real_addr].pend_write += 1;
        else
            vis[real_addr].pend_read += 1;
    }

    return ret;
}

void dramsim3_wrapper::ClockTick(){
    ms->ClockTick();
}


void dramsim3_wrapper::read_callback(uint64_t addr){
    if(vis[addr].pend_read <= 0) {
        std::cerr << "read callback failed: extra read callback" << std::endl;
        std::exit(1);
    } else {
        vis[addr].pend_read -= 1;
    }

    return;

}
void dramsim3_wrapper::write_callback(uint64_t addr){
    if(vis[addr].pend_write <= 0) {
        std::cerr << "write callback failed: extra write callback" << std::endl;
        std::exit(1);
    } else {
        vis[addr].pend_write -= 1;
    }

    return;
}

bool dramsim3_wrapper::drained() {
    for(auto &i: vis) {
        if(i.second.pend_read != 0 || i.second.pend_write != 0)
            return false;
    }

    return true;
}

int dramsim3_wrapper::get_pend_read(uint64_t addr, bool is_pim){
    uint64_t real_addr;
    if(is_pim) {
        real_addr = ms->BankLocalToGlobalAddr(ch_, ra_, bg_, ba_, addr);
    } else {
        real_addr = addr;
    }
    auto it = vis.find(real_addr);
    if(it == vis.end())
        return 0;
    else
        return it->second.pend_read;
}
int dramsim3_wrapper::get_pend_write(uint64_t addr, bool is_pim){
    uint64_t real_addr;
    if(is_pim) {
        real_addr = ms->BankLocalToGlobalAddr(ch_, ra_, bg_, ba_, addr);
    } else {
        real_addr = addr;
    }
    auto it = vis.find(real_addr);
    if(it == vis.end())
        return 0;
    else
        return it->second.pend_write;
}


uint64_t dramsim3_wrapper::BankLocalToGlobalAddr(uint64_t channel, uint64_t rank,
                                        uint64_t bankgroup, uint64_t bank,
                                        uint64_t hex_addr) const{
    return ms->BankLocalToGlobalAddr(channel, rank, bankgroup, bank, hex_addr);
}

uint64_t dramsim3_wrapper::ExactLocalToGlobalAddr(uint64_t channel, uint64_t rank, uint64_t bankgroup, uint64_t bank, uint64_t ro, uint64_t co) const {
    return ms->ExactLocalToGlobalAddr(channel, rank, bankgroup, bank, ro, co);
}
void gen_trace::generate() {
    if (!outfile.is_open()) {
        std::cerr << "Error: Output file is not open." << std::endl;
        return;
    }

    uint64_t current_timing = 0;

    // DRAM mapping parameters
    const size_t ROW_SIZE = 0x7f;
    const size_t MAX_ROWS = 0x1ffff;

    // State tracking variables
    size_t stream_addr = 0;

    // MIX configuration parameters
    const int HIT_CHUNK_SIZE = 16;
    const int MISS_CHUNK_SIZE = 4;
    bool generating_hits = true;
    int current_chunk_count = 0;

    size_t row_addr = 0, col_addr = 0;

    // Random generators for RW ratio, Rows, and Columns
    std::mt19937 gen(1337);
    std::uniform_int_distribution<int> dist_rw(0, 99);

    // Use uniform distribution to pick random rows and columns
    std::uniform_int_distribution<size_t> dist_row(0, MAX_ROWS - 1);
    std::uniform_int_distribution<size_t> dist_col(0, ROW_SIZE - 1);

    for (int i = 0; i < num; ++i) {

        // 1. Generate Address based on Pattern
        if (typ == TraceType::STREAM) {
            row_addr = 0; // Pin to a single row
            col_addr = stream_addr;

            stream_addr++;
            if (stream_addr > ROW_SIZE) { // Prevent overflowing the column mask
                stream_addr = 0;
            }
        }
        else if (typ == TraceType::RANDOM) {
            // Scatter across completely random rows and columns
            row_addr = (row_addr+1)%ROW_SIZE;
            col_addr = dist_col(gen);
        }
        else if (typ == TraceType::MIX) {
            if (generating_hits) {
                if (current_chunk_count == 0) {
                    // Start a new STREAM chunk on a random row
                    row_addr = dist_row(gen);
                    col_addr = 0;
                } else {
                    // Keep the same row, increment the column to create a stream
                    col_addr++;
                    if (col_addr > ROW_SIZE) {
                        col_addr = 0;
                    }
                }

                current_chunk_count++;
                if (current_chunk_count >= HIT_CHUNK_SIZE) {
                    generating_hits = false;
                    current_chunk_count = 0;
                }
            } else {
                // Generate RANDOM accesses (misses)
                row_addr = dist_row(gen);
                col_addr = dist_col(gen);

                current_chunk_count++;
                if (current_chunk_count >= MISS_CHUNK_SIZE) {
                    generating_hits = true;
                    current_chunk_count = 0;
                }
            }
        }

        // 2. Map Local to Global Address
        // Now row_addr and col_addr are guaranteed to be correct for all 3 types
        uint64_t global_addr = ms->ExactLocalToGlobalAddr(
            ms->get_channel(),
            ms->get_rank(),
            ms->get_bankgroup(),
            ms->get_bank(),
            row_addr,
            col_addr
        );

        // 3. Determine Read or Write
        std::string rw_cmd;
        if (all_read) {
            rw_cmd = "READ";
        } else if (all_write) {
            rw_cmd = "WRITE";
        } else {
            if (dist_rw(gen) < rw_ratio) {
                rw_cmd = "READ";
            } else {
                rw_cmd = "WRITE";
            }
        }

        // 4. Write to File
        outfile << "0x" << std::hex << global_addr << std::dec << " "
                << rw_cmd << " "
                << current_timing << "\n";

        // 5. Increment Timing
        current_timing += t_interval;
    }

    outfile.flush();
}
