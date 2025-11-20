#include <cassert>
#include <cstdio>
#include <iostream>
#include <vector>
#include <cstring>
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
                (fatptr){0xbbc, 0},
            }
        },
        (instruction_t){
            Opcode::NOP,
            0xdead,
            0xdead,
            {
            }
        },
        (instruction_t){
            Opcode::EXIT,
            0,
            0,
            {
                (fatptr){0xcca, 0},
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

    pimCopyHostToDevice((void*)cpu->mem.data(), a_id, 0UL, 0UL);
    pimCopyHostToDevice((void*)(cpu->mem.data() + sizeof(int) * 32), b_id, 0UL, 0UL);

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


void init_pimdev(bool has_hmt){
    /*
     * For some reason, PIM_DEVICE_BANK_LEVEL will eat too much memory and trigger OOM killer
     */
    PimStatus status = pimCreateDeviceFromConfig_HMT(PIM_FUNCTIONAL, "cfg/PIMeval_Bank_Rank1.cfg", has_hmt);
    if (status != PIM_OK)
    {
      std::cout << "init_pimdev(): Abort" << std::endl;
      return;
    }
}

int main(int argc, char **argv) {
    bool has_hmt = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--hmt") == 0) {
            has_hmt = true;
            break;
        }
    }

    
    init_pimdev(has_hmt);

    IMEM_t *pim_imem = new IMEM_t();
    init_pim_imem(pim_imem);

    HMT_table_t *pim_hmt = new HMT_table_t();
    cpu_t *pim_cpu = new cpu_t(pim_imem, pim_hmt);

    init_pim_hmt(pim_cpu, pim_hmt);

    for(int i=0; i<100; i++){
        if(pim_cpu->cpu_stop == true){
            printf("Program exit\n");
            break;
        }
        pim_cpu->tick();
    }

#ifdef PIM_FUSE
    pim_cpu->exec_fuse();
#endif
    /*
     * TODO
     * pimFree() all memory after finished
     */
    pimShowStats();

    int *result = (int*)calloc(1, sizeof(int) * 128);
    PimStatus ret = pimCopyDeviceToHost(0, (void*)result, 0UL, 0UL);
    assert(ret == PIM_OK);
    ret = pimCopyDeviceToHost(0, (void*)(result + sizeof(int) * 32), 0UL, 0UL);
    assert(ret == PIM_OK);

    assert(result[15] == 3); // random check for result

    printf("Finished\n");

    return 0;
}
