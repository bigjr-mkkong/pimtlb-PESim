#ifndef __CPU__H__
#define __CPU__H__

#include <cassert>
#include <vector>
#include "HMT.h"
#include "libpimeval.h"

enum Opcode{
    MUL,
    ADD,
    SCALED_ADD,
    EXIT,
    JUMP,
    NOP
};

struct fatptr{
    size_t varidx;
    size_t offset;
};

struct instruction_t{
    Opcode opcode;
    int imm0, imm1;
    std::vector<fatptr> oprands;
};

class IMEM_t{
    public:
    size_t max_pc;
    std::vector<instruction_t> insts;

    IMEM_t(): max_pc(0){};
    instruction_t &get_by_pc(size_t pc);
    void push_inst(instruction_t &inst);
};

class cpu_t{
    size_t pc, next_pc;
    size_t clock_cntr, next_clock_cntr;
    instruction_t readed_inst;
    bool readed_inst_valid;
    bool need_jump;

    bool deco_flush;
    bool fl_hold;

    PimFusionBlock *fused;
    
    IMEM_t *imem;
    HMT_table_t *hmt;

    public:
    bool cpu_stop;
    std::vector<uint8_t> mem;
    cpu_t(IMEM_t *imem, HMT_table_t *hmt, PimFusionBlock *progs):\
        pc(0),\
        readed_inst_valid(false),\
        clock_cntr(0),\
        cpu_stop(false),\
        imem(imem),\
        need_jump(false),\
        fl_hold(false),\
        hmt(hmt){
            fused = progs;
        };

    void read_inst();
    void deco_inst();
    void tick();
    
    void exec_fuse(){
        PimStatus result = pimFuse(*fused);
        assert(result == PIM_OK);
    }
    
};

#endif
