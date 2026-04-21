#ifndef __DRAMSIM3_WRAPPER_H__
#define __DRAMSIM3_WRAPPER_H__

#include "pesim-configs.h"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <string>
#include <fstream>
#include <sstream>


#include "memory_system.h"

class dramsim3_wrapper{
    public:
    struct vis_state_t{
        int pend_read = 0;
        int pend_write = 0;
    };
    dramsim3_wrapper(int ch, int ra, int bg, int ba, bool is_pim);
    bool WillAcceptTransaction(uint64_t hex_addr, bool is_write) const;
    bool AddTransaction(uint64_t hex_addr, bool is_write, bool is_pim = false);
    void ClockTick();
    bool drained();
    int get_pend_read(uint64_t addr, bool is_pim);
    int get_pend_write(uint64_t addr, bool is_pim);

    uint64_t BankLocalToGlobalAddr(uint64_t channel, uint64_t rank,
                                        uint64_t bankgroup, uint64_t bank,
                                        uint64_t hex_addr) const;

    void GlobalToLocalAddr(uint64_t* channel, uint64_t* rank, uint64_t* bankgroup, uint64_t* bank, uint64_t* local_addr, uint64_t hex_addr) const;

    //Only use this function when generating address with same row but different col
    uint64_t ExactLocalToGlobalAddr(uint64_t channel, uint64_t rank, uint64_t bankgroup, uint64_t bank, uint64_t ro, uint64_t co) const;
    const int get_channel() const {return ch_;}
    const int get_rank()  const {return ra_;}
    const int get_bankgroup() const {return bg_;}
    const int get_bank() const {return ba_;}

    protected:
    std::unique_ptr<dramsim3::MemorySystem> ms;
    std::unordered_map<uint64_t, vis_state_t> vis;

    void read_callback(uint64_t addr);
    void write_callback(uint64_t addr);



    int ch_, ra_, bg_, ba_;

};

class gen_trace{
    public:
    enum TraceType{
        STREAM,
        RANDOM,
        MIX,
        UNDEF,
    };

    std::string toString(TraceType typ){
        switch(typ) {
            case(TraceType::MIX):
                return "MIX";
            case(TraceType::RANDOM):
                return "RANDOM";
            case(TraceType::STREAM):
                return "STREAM";
            default:
                return "UNDEF";
        }
    }
    
    gen_trace(TraceType typ_, int num_, int t_interval_, int rw_ratio_, bool all_read_, bool all_write_,const dramsim3_wrapper *ms_): 
        typ(typ_),
        num(num_), 
        t_interval(t_interval_),
        rw_ratio(rw_ratio_),
        all_read(all_read_),
        all_write(all_write_),
        ms(ms_){
            std::ostringstream ss;
            ss<<"dramsim3-"<<toString(typ_)<<".trace";
            std::string fname = ss.str();
            outfile.open(fname);
        }

    void generate();

    private:
    TraceType typ;
    int num;
    int t_interval;
    int rw_ratio;
    bool all_read;
    bool all_write;
    const dramsim3_wrapper *ms;

    const size_t LOCAL_MAX = 0xffffff;
    std::ofstream outfile;
};

#endif
