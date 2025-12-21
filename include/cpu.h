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
    BOUND_JUMP,
    SET_BOUND,
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

    int IFID_HMT_varid_dst, IFID_HMT_varid_src0, IFID_HMT_varid_src1,\
        IFID_HMT_imm0;
    Opcode IFID_HMT_opc;
    bool IFID_HMT_valid;

    PimObjId HMT_RCW_src0, HMT_RCW_src1, HMT_RCW_dst, HMT_RCW_imm0;
    Opcode HMT_RCW_opc;
    bool HMT_RCW_valid;

    bool need_jump;

    PimFusionBlock *fused;
    
    IMEM_t *imem;
    HMT_table_t *hmt;

    public:
    bool cpu_stop;
    std::vector<uint8_t> mem;
    cpu_t(IMEM_t *imem, HMT_table_t *hmt, PimFusionBlock *progs):\
        pc(0),\
        clock_cntr(0),\
        cpu_stop(false),\
        imem(imem),\
        need_jump(false),\
        IFID_HMT_valid(false),\
        HMT_RCW_valid(false),\
        hmt(hmt){
            fused = progs;
        };

    void RCW();
    void read_inst();
    void deco_inst();
    void hmt_check();
    void tick();
    
    void exec_fuse(){
        PimStatus result = pimFuse(*fused);
        assert(result == PIM_OK);
    }

    bool stopable();
    
};

#endif
