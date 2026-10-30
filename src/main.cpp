// Entry point for the six student-owned modules.
// This build uses one translation unit to remain compatible with the
// existing inline class definitions and GCC 6.3. Compile main.cpp only.
#include "Common.h"
#include "Vehicle_Lane.cpp"
#include "Signal_Adaptive.cpp"
#include "Vehicle_Detection.cpp"
#include "Emergency_Routing.cpp"
#include "Analytics_Reports.cpp"
#include "Simulation_Engine.cpp"

int main() {
    do {
        TrafficSimulation simulation;
        simulation.run();
    } while (readYesNo("\nRun another simulation?"));

    cout << "\nGoodbye.\n";
    return 0;
}