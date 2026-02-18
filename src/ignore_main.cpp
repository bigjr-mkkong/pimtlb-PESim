#include <iostream>
#include "PESim.h"

#ifdef  MAIN_TEST
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    SimdSim sim;
    sim.cpu().set_vreg(1, {0, 1, 2, 3});
    sim.cpu().set_vreg(2, {1, 1, 1, 1});
    sim.run(50);
    auto result = sim.cpu().get_vreg(1);
    std::cout << "ADD result lane1: " << result[0] << std::endl;

    return 0;
}
#endif
