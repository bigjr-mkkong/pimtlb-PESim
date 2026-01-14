#include <iostream>
#include "Sim.h"

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    SimdSim sim;
    sim.cpu().set_vreg(1, {1, 2, 3, 4});
    sim.cpu().set_vreg(2, {10, 20, 30, 40});
    sim.run(10);
    auto result = sim.cpu().get_vreg(0);
    std::cout << "ADD result lane0: " << result[0] << std::endl;

    return 0;
}
