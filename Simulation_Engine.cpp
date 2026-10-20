//Module 6:: Simulation Engine

#include "Common.h"

enum LaneIdentifier
{
    NORTH_LANE = 1,
    SOUTH_LANE = 2,
    EAST_LANE = 3,
    WEST_LANE = 4
};

class TrafficSimulation 
{
private:
    Lane north;
    Lane south;
    Lane east;
    Lane west;

    PhaseController signalController;
    TrafficAnalyzer analyzer;
    EmergencyManager emergencyManager;
    GreenCorridor greenCorridor;
    PerformanceAnalytics analytics;
    VehicleDetector detector;

    int simulationClock;
    int totalInitialVehicles;
    int emergencyRouteFailures;

    // Index 0..3 = North, South, East, West
    int laneCounts[4];
    int laneStartId[4];

    Lane& laneByCode(int code) {
        if (code == NORTH_LANE) return north;
        if (code == SOUTH_LANE) return south;
        if (code == EAST_LANE) return east;
        return west;
    }

    int northSouthCount() const {
        return north.getCount() + south.getCount();
    }

    int eastWestCount() const {
        return east.getCount() + west.getCount();
    }

    int normalRemaining() const {
        return northSouthCount() + eastWestCount();
    }

    // Which lane (0..3) did this vehicle ID come from?
    int laneOfVehicle(int id) const {
        for (int lane = 0; lane < 4; lane++) {
            if (id > laneStartId[lane] &&
                id <= laneStartId[lane] + laneCounts[lane]) {
                return lane;
            }
        }

        return 0;
    }

    // ---------- STEP 1: DETECTION ----------

    void detectTraffic() {
        const char* names[4] = {"North", "South", "East", "West"};
        int emergencyCounts[4];
        int usedPositions[4][VehicleDetector::MAX_EMERGENCY_PER_LANE];

        // Sensor 1: vehicle count per lane.
        int offset = 0;

        for (int lane = 0; lane < 4; lane++) {
            laneCounts[lane] = detector.detectVehicles(names[lane]);
            laneStartId[lane] = offset;
            offset += laneCounts[lane];
        }

        totalInitialVehicles = offset;

        // Create the vehicle objects. IDs run 1..N across all lanes.
        for (int lane = 0; lane < 4; lane++) {
            for (int i = 0; i < laneCounts[lane]; i++) {
                VehicleType kind = static_cast<VehicleType>(i % 4);
                laneByCode(lane + 1).addVehicle(
                    Vehicle(laneStartId[lane] + i + 1, kind, 0));
            }
        }

        // Sensor 2: how many sirens in each lane.
        for (int lane = 0; lane < 4; lane++) {
            emergencyCounts[lane] =
                detector.detectEmergencies(names[lane], laneCounts[lane]);
        }

        // For every siren: which vehicle is it? Move it out of the
        // normal queue and into the emergency priority heap.
        for (int lane = 0; lane < 4; lane++) {
            for (int k = 0; k < emergencyCounts[lane]; k++) {
                int position = detector.detectEmergencyPosition(
                    laneCounts[lane], usedPositions[lane], k);
                usedPositions[lane][k] = position;

                int globalId = laneStartId[lane] + position;
                Vehicle removed;
                bool found = laneByCode(lane + 1).extractVehicleById(
                    globalId, removed);

                if (found) {
                    emergencyManager.insertEmergency(
                        Vehicle(globalId, EMERGENCY, 0));
                }
            }
        }
    }

    void displayInitialQueues() const {
        cout << "\n--- INITIAL TRAFFIC AFTER EMERGENCY TRANSFER ---\n";
        cout << "North normal queue: " << north.getCount() << '\n';
        cout << "South normal queue: " << south.getCount() << '\n';
        cout << "East normal queue:  " << east.getCount() << '\n';
        cout << "West normal queue:  " << west.getCount() << '\n';
    }

    // ---------- STEP 2: EMERGENCY GREEN CORRIDOR ----------

    void processEmergencyVehicles() {
        if (emergencyManager.isEmpty()) {
            return;
        }

        // Each lane has an entry point. In Phase II, all emergency
        // vehicles use one common destination: City Hospital (node 0).
        const int entryByLane[4] = {1, 3, 8, 5};
        const char* laneNames[4] = {"North", "South", "East", "West"};

        cout << "\n--- GREEN CORRIDOR ACTIVE ---\n";

        signalController.allRed();
        cout << "North-South signal: RED\n";
        cout << "East-West signal: RED\n";
        cout << "Emergency vehicles waiting: "
             << emergencyManager.getCount() << '\n';

        Vehicle emergency;

        while (emergencyManager.extractHighestPriority(emergency)) {
            int lane = laneOfVehicle(emergency.getId());
            int start = entryByLane[lane];
            int destination = 0; // Common destination in Phase II

            int route[MAX_INTERSECTIONS];
            int routeLength = 0;

            // operator<< of Vehicle prints type, ID, arrival time and priority.
            cout << "\n" << emergency << " from "
                 << laneNames[lane] << " lane\n";

            if (!greenCorridor.findShortestRoute(start, destination,
                                                 route, routeLength)) {
                emergencyRouteFailures++;
                cout << "No open route. Vehicle needs an alternative plan.\n";
                continue;
            }

            cout << "Route: ";

            for (int i = 0; i < routeLength; i++) {
                cout << greenCorridor.getName(route[i]);

                if (i < routeLength - 1) {
                    cout << " -> ";
                }
            }

            int distance = greenCorridor.pathDistance(route, routeLength);
            cout << " (" << distance << " units)\n";

            // One distance unit = one second of corridor clearance.
            int serviceTime = simulationClock + distance + ALL_RED_TIME;
            int waitTime = serviceTime - emergency.getArrivalTime();

            analytics.addRecord(emergency.getId(), 0, serviceTime,
                                waitTime, true);
            simulationClock = serviceTime;

            cout << "Emergency vehicle " << emergency.getId()
                 << " served through Green Corridor.\n";
        }

        cout << "Green Corridor is now CLEAR.\n";
        cout << "Normal traffic may resume.\n";
    }

    // ---------- STEP 3: NORMAL TRAFFIC ----------

    // Lanes of one direction move side by side: every lane can pass
    // one vehicle every SECONDS_PER_VEHICLE seconds.
    int serveLane(Lane& lane, int laneCode, int slots) {
        int served = 0;
        Vehicle vehicle;

        while (served < slots && lane.serveVehicle(vehicle)) {
            served++;

            int serviceTime = simulationClock
                              + served * analyzer.getSecondsPerVehicle();
            int waitTime = serviceTime - vehicle.getArrivalTime();

            analytics.addRecord(vehicle.getId(), laneCode, serviceTime,
                                waitTime, false);
        }

        return served;
    }

    void runPhase(TrafficPhase phase, int greenTime) {
        const char* phaseName = (phase == NORTH_SOUTH_PHASE)
                                    ? "NORTH-SOUTH" : "EAST-WEST";
        Lane& first  = (phase == NORTH_SOUTH_PHASE) ? north : east;
        Lane& second = (phase == NORTH_SOUTH_PHASE) ? south : west;
        int firstCode  = (phase == NORTH_SOUTH_PHASE) ? NORTH_LANE : EAST_LANE;
        int secondCode = (phase == NORTH_SOUTH_PHASE) ? SOUTH_LANE : WEST_LANE;

        cout << '\n';

        if (first.isEmpty() && second.isEmpty()) {
            cout << phaseName << " has no waiting vehicles. Phase skipped.\n";
            return;
        }

        signalController.activatePhase(phase);

        cout << phaseName << " GREEN for " << greenTime << " seconds\n";
        cout << "Other signal: RED\n";

        int slots = greenTime / analyzer.getSecondsPerVehicle();
        int servedFirst = serveLane(first, firstCode, slots);
        int servedSecond = serveLane(second, secondCode, slots);

        cout << first.getName() << ": " << servedFirst
             << " vehicles served\n";
        cout << second.getName() << ": " << servedSecond
             << " vehicles served\n";

        cout << phaseName << " YELLOW\n";
        signalController.finishPhase();
        cout << phaseName << " RED\n";

        simulationClock += greenTime + PhaseController::transitionTime();
    }

    void runSignalCycle() {
        // Green time follows the longest lane of each direction.
        int nsGreen = analyzer.calculateGreenTime(
            north.getCount() > south.getCount() ? north.getCount()
                                                : south.getCount());
        int ewGreen = analyzer.calculateGreenTime(
            east.getCount() > west.getCount() ? east.getCount()
                                              : west.getCount());

        TrafficPhase first = analyzer.choosePhase(northSouthCount(),
                                                  eastWestCount());
        TrafficPhase second = (first == NORTH_SOUTH_PHASE)
                                  ? EAST_WEST_PHASE : NORTH_SOUTH_PHASE;

        cout << "\nSelected phase: "
             << (first == NORTH_SOUTH_PHASE ? "NORTH-SOUTH" : "EAST-WEST")
             << '\n';
        cout << "North-South green time: " << nsGreen << " seconds\n";
        cout << "East-West green time: " << ewGreen << " seconds\n";

        runPhase(first, first == NORTH_SOUTH_PHASE ? nsGreen : ewGreen);
        runPhase(second, second == NORTH_SOUTH_PHASE ? nsGreen : ewGreen);
    }

    // ---------- REPORTS ----------

    void displayFinalQueues() const {
        cout << "\n--- FINAL QUEUE REPORT ---\n";
        cout << "North: " << north.getCount() << '\n';
        cout << "South: " << south.getCount() << '\n';
        cout << "East:  " << east.getCount() << '\n';
        cout << "West:  " << west.getCount() << '\n';
    }

    void displaySummary() const {
        int totalServed = analytics.getNormalServed()
                          + analytics.getEmergencyServed();

        cout << "\n--- SIMULATION PERFORMANCE SUMMARY ---\n";
        cout << "Initial vehicles: " << totalInitialVehicles << '\n';
        cout << "Emergency vehicles served: "
             << analytics.getEmergencyServed() << '\n';
        cout << "Normal vehicles served: "
             << analytics.getNormalServed() << '\n';
        cout << "Total vehicles served: " << totalServed << '\n';
        cout << "Normal vehicles remaining: " << normalRemaining() << '\n';
        cout << "Total vehicles remaining: "
             << normalRemaining() + emergencyManager.getCount() << '\n';
        cout << "Emergency route failures: "
             << emergencyRouteFailures << '\n';
        cout << "Vehicle objects created: "
             << Vehicle::getCreatedCount() << '\n';
    }

    // Polymorphism: one loop calls each module's own displayReport().
    void displayAllReports() const {
        const Reportable* reports[] = {
            &north, &south, &east, &west,
            &signalController, &analyzer, &emergencyManager,
            &greenCorridor, &analytics
        };

        printHeader("ALL MODULE REPORTS");

        const char* lastModule = "";

        for (int i = 0; i < 9; i++) {
            if (strcmp(lastModule, reports[i]->moduleName()) != 0) {
                cout << "\n-- " << reports[i]->moduleName() << " --\n";
                lastModule = reports[i]->moduleName();
            }

            reports[i]->displayReport();
        }
    }

    void detailedAnalytics() {
        analytics.displayServiceHistory(15);
        analytics.displayReport();

        if (analytics.getRecordCount() > 0 &&
            readYesNo("\nSort history by wait time?")) {
            analytics.displaySortedHistory();
        }

        if (readYesNo("\nShow all module reports?")) {
            displayAllReports();
        }
    }

public:
    TrafficSimulation()
        : north("North"),
          south("South"),
          east("East"),
          west("West") {
        simulationClock = 0;
        totalInitialVehicles = 0;
        emergencyRouteFailures = 0;

        for (int i = 0; i < 4; i++) {
            laneCounts[i] = 0;
            laneStartId[i] = 0;
        }

        // Green time = 3 s per vehicle of the longest lane,
        // between 10 and 60 seconds.
        analyzer.setGreenLimits(10, 60);
        greenCorridor.setupDemoNetwork();
    }

    void run() {
        cout << "\n=== SMART TRAFFIC LIGHT OPTIMISER ===\n\n";

        detector.chooseMode();
        detectTraffic();

        displayInitialQueues();
        processEmergencyVehicles();

        int cycle = 1;

        while (true) {
            if (cycle > 1) {
                cout << "\n=== SIGNAL CYCLE " << cycle << " ===\n";
            }

            runSignalCycle();
            displayFinalQueues();
            displaySummary();

            if (normalRemaining() == 0) {
                cout << "\nAll normal vehicles have been served.\n";
                break;
            }

            if (!readYesNo("\nRun another signal cycle?")) {
                break;
            }

            cycle++;
        }

        if (readYesNo("\nShow detailed analytics (Module 5)?")) {
            detailedAnalytics();
        }

        cout << "\n=== SIMULATION COMPLETED ===\n";
    }
};