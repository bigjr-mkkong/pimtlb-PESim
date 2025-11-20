#include <cassert>
#include <cstdio>
#include <iostream>
#include <vector>
#include <cstring>
#include "HMT.h"
#include "Sim.h"
#include "cpu.h"
#include "libpimeval.h"

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

    
    Sim pim_sim;

    init_pimdev(has_hmt);
    pim_sim.mb_fill();
    pim_sim.init_mem();
    pim_sim.init_imem();
    pim_sim.init_cpu();
    pim_sim.pre_sim();
    pim_sim.sim_begin(100);
    pim_sim.post_sim();

    printf("Finished\n");

    return 0;
}
