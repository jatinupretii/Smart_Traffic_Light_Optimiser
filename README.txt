SMART TRAFFIC LIGHT OPTIMISER — SIX-MEMBER MODULE SPLIT
=====================================================

This version reorganises the existing Phase-II code into six student-owned
modules with clear responsibilities. It preserves the existing single-
translation-unit build style so it remains compatible with MinGW GCC 6.3.0.

MODULE OWNERSHIP
----------------
1. Module1_Vehicle_Lane.cpp
   Vehicle types/priority, Vehicle class, Lane circular queue, enqueue/dequeue,
   queue status and lane report.

2. Module2_Signal_Adaptive.cpp
   Traffic signal states, phase controller, traffic phase selection, adaptive
   green-time calculation and green-time limits.

3. Module3_Vehicle_Detection.cpp
   Manual/random simulated sensor input, vehicle counts and emergency-position
   detection.

4. Module4_Emergency_Routing.cpp
   Emergency max-heap, emergency priority and Green Corridor graph/BFS routing.

5. Module5_Analytics_Reports.cpp
   Service history dynamic array, vehicle-history search, bubble-sort report,
   waiting-time and service analytics.

6. Module6_Simulation_Engine.cpp
   Integrates modules, builds initial queues, processes emergencies, runs signal
   cycles, serves traffic and prints simulation summaries/reports.

BUILD (PowerShell / Windows)
----------------------------
Open a terminal in this folder and run:
    g++ -std=c++14 main.cpp -o traffic_simulation.exe
    .\traffic_simulation.exe

IMPORTANT
---------
Compile main.cpp only. The module .cpp files are included by main.cpp; do not
pass all module .cpp files separately to g++, or duplicate definitions will occur.

The code has been reorganised by responsibility, not padded with filler code.
The simulation engine naturally contains orchestration logic, so exact line-count
equality is not a useful target; the six ownership areas are cohesive and explainable.
