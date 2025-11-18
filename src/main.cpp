#include <cassert>
#include <cstdio>
#include <iostream>
#include <vector>
#include "HMT.h"
#include "cpu.h"
#include "libpimeval.h"

void init_pim_imem(IMEM_t *pim_imem) {
    pim_imem->insts = {
        (instruction_t){
            Opcode::ADD,
            0xdead,
            0xdead,
            {
                (fatptr){0xcca, 0},
                (fatptr){0xbbc, 0}
            }
        },
        (instruction_t){
            Opcode::EXIT,
            0,
            0,
            {
            }
        },
    };

    pim_imem->max_pc = 4;
}

void init_memory(cpu_t *cpu) {
    cpu->mem.reserve(128 * sizeof(int));
    int *ptr = (int*)(cpu->mem.data());
    for(int i=0; i<32; i++) {
        ptr[i] = 1;
    }
    for(int i=32; i<64; i++) {
        ptr[i] = 2;
    }
}

void init_pim_hmt(cpu_t *cpu, HMT_table_t *hmt){
    //pimAlloc for each variable in imem
    //Add the PimObjID from each pimAlloc into HMT, with size and access flag

    /*
     * memory:
     * a: 0-31: 32 * int
     * b: 32-63: 32 * int
     */

    init_memory(cpu);

    PimObjId a_id = pimAlloc(PIM_ALLOC_AUTO, 32, PIM_INT32);
    if(a_id == -1) {
        std::cerr<<"init_pim_hmt(): Failed to allocate for array a"<<std::endl;
        exit(1);
    }

    PimObjId b_id = pimAllocAssociated(a_id, PIM_INT32);
    if(b_id == -1) {
        std::cerr<<"init_pim_hmt(): Failed to allocate for array b"<<std::endl;
        exit(1);
    }

    hmt->add_new_ent(\
            (size_t)(0),\
            0xcca,\
            32 * sizeof(int),\
            a_id,\
            HMT_flag_t::RW);


    hmt->add_new_ent(\
            (size_t)(32 * sizeof(int)),\
            0xbbc,\
            32 * sizeof(int),\
            b_id,\
            HMT_flag_t::RW);

    return;
}


void init_pimdev(){
    unsigned numRanks = 4;
    unsigned numBankPerRank = 128; // 8 chips * 16 banks
    unsigned numSubarrayPerBank = 32;
    unsigned numRows = 1024;
    unsigned numCols = 8192;

    PimStatus status = pimCreateDevice(PIM_FUNCTIONAL, numRanks, numBankPerRank, numSubarrayPerBank, numRows, numCols);
    if (status != PIM_OK)
    {
      std::cout << "init_pimdev(): Abort" << std::endl;
      return;
    }
}

int main(void) {
    init_pimdev();


    IMEM_t *pim_imem = new IMEM_t();
    init_pim_imem(pim_imem);

    HMT_table_t *pim_hmt = new HMT_table_t();
    cpu_t *pim_cpu = new cpu_t(pim_imem, pim_hmt);

    init_pim_hmt(pim_cpu, pim_hmt);

    for(int i=0; i<100; i++){
        if(pim_cpu->cpu_stop == true){
            printf("Program exit before run out of time :)\n");
            break;
        }
        pim_cpu->tick();
    }

    printf("Finished\n");

    return 0;
}
