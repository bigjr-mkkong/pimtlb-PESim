#ifndef __CPU__H__
#define __CPU__H__

#include <vector>
#include "HMT.h"
#include "libpimeval.h"

enum Opcode{
    LOAD,
    STORE,
    MUL,
    ADD
};

struct fatptr{
    size_t varidx;
    size_t offset;
    HMT_flag_t flag;
};

struct instruction_t{
    Opcode opcode;
    int reg;
    std::vector<fatptr> oprands;
};

class IMEM_t{
    std::vector<instruction_t> insts;
    size_t max_pc;

    public:
    IMEM_t(): max_pc(0){};
    instruction_t &get_by_pc(size_t pc);
    void push_inst(instruction_t &inst);
};

class cpu_t{
   size_t pc, next_pc;
   size_t clock_cntr, next_clock_cntr;
   instruction_t readed_inst;

   PimObjId rf[16];
   IMEM_t &imem;
   std::vector<uint8_t> mem;
   HMT_table_t hmt;

   public:
   cpu_t(IMEM_t &imem): pc(0), clock_cntr(0), imem(imem){};

   void read_inst();
   void deco_inst();
   void tick();

};

#endif
