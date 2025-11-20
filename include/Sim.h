#ifndef __SIM_H__
#define __SIM_H__

#include "libpimeval.h"
#include "HMT.h""
#include "cpu.h""
#include <vector>

#define SIM_NOTRACE
// #define PIM_FUSE

enum mb_type{
    INPUT,
    OUTPUT,
    ANON
};

struct mem_bond{

    size_t varidx;

    PimObjId pim_id;
    void *base;
    size_t size;
    mb_type typ;
};

class Sim{
    std::vector<mem_bond> mem_list;
    cpu_t *sim_cpu;
    IMEM_t *sim_imem;
    HMT_table_t *sim_hmt;

    PimFusionBlock *fused_prog;

    public:
    Sim(){
        fused_prog = new PimFusionBlock;
    }
    void mb_fill();
    void init_mem();
    void init_imem();
    void init_cpu();

    void pre_sim();
    void sim_begin(size_t max_cycle);
    void post_sim();
};

#endif
