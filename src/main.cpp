#include <iostream>
#include "Sim.h"

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    SimdSim sim;
    sim.cpu().set_vreg(1, {1, 2, 3, 4});
    sim.cpu().set_vreg(2, {10, 20, 30, 40});
    sim.run(50);
    auto result = sim.cpu().get_vreg(1);
    std::cout << "ADD result lane1: " << result[1] << std::endl;

    return 0;
}
