#include "Common.h"
#include "Vehicle_Lane.cpp"
#include "Signal_Adaptive.cpp"
#include "Vehicle_Detection.cpp"
#include "Emergency_Routing.cpp"
#include "Analytics_Reports.cpp"
#include "Simulation_Engine.cpp"

void testModule1()
{
    printHeader("MODULE 1 - VEHICLE & LANE MANAGEMENT");

    Lane north("North");
    Lane south("South");
    Lane east("East");
    Lane west("West");

    Lane* lanes[4] = { &north, &south, &east, &west };
    int nextVehicleId = 1;

    while (true)
    {
        cout << "\nSelect lane:\n";
        cout << "1. North\n";
        cout << "2. South\n";
        cout << "3. East\n";
        cout << "4. West\n";
        cout << "0. Finish adding vehicles\n";

        int laneChoice = readInt("Enter lane number: ", 0, 4);

        if (laneChoice == 0)
        {
            break;
        }

        Lane* lane = lanes[laneChoice - 1];

        int count = readInt("How many vehicles to add (1-10)? ", 1, 10);

        for (int i = 1; i <= count; i++)
        {
            int typeChoice = readInt(
                "Vehicle type: 0=Car, 1=Bus, 2=Motorbike, 3=Truck, 4=Emergency: ",
                0, 4
            );

            VehicleType type = static_cast<VehicleType>(typeChoice);
            Vehicle vehicle(nextVehicleId, type, 0);

            if (lane->addVehicle(vehicle))
            {
                cout << "Added to " << lane->getName()
                     << ": " << vehicle << '\n';

                nextVehicleId++;
            }
            else
            {
                cout << "Lane is full. Vehicle could not be added.\n";
            }
        }

        cout << "\nUpdated lane status:\n";
        lane->displayQueue();
        // Progress bar display omitted to maintain clean text-based console tracking logs
        lane->displayReport();
    }

    cout << "\n============================================\n";
    cout << "             FINAL LANE STATUS\n";
    cout << "============================================\n";

    for (int i = 0; i < 4; i++)
    {
        lanes[i]->displayQueue();
        lanes[i]->displayReport();
        cout << '\n';
    }

    int serveChoice = readInt(
        "Select lane to serve (1=North, 2=South, 3=East, 4=West, 0=Skip): ",
        0, 4
    );

    if (serveChoice != 0)
    {
        Lane* selectedLane = lanes[serveChoice - 1];
        cout << "\n=== DISCHARGING ENTIRE " << selectedLane->getName() << " LANE ===\n";

        if (selectedLane->isEmpty())
        {
            cout << "Selected lane is empty. No vehicle served.\n";
        }
        else
        {
            int servedCount = 0;
            Vehicle served;

            // Batch Processing Loop: Continues clearing standard queues until empty
            while (!selectedLane->isEmpty())
            {
                if (selectedLane->serveVehicle(served))
                {
                    servedCount++;
                    cout << "Served [" << servedCount << "] from " << selectedLane->getName()
                         << ": " << served << '\n';
                }
            }
            cout << "Successfully cleared all " << servedCount << " vehicles from the lane.\n";
        }
    }

    cout << "\n============================================\n";
    cout << "          FINAL LANE REPORTS\n";
    cout << "============================================\n";

    for (int i = 0; i < 4; i++)
    {
        lanes[i]->displayQueue();
        lanes[i]->displayReport();
        cout << '\n';
    }
}

void testModule2()
{
    printHeader("MODULE 2 - SIGNAL CONTROL & ADAPTIVE TIMING");

    PhaseController controller;
    TrafficAnalyzer analyzer;
    int northSouth = readInt("Vehicles in North-South direction (0-200): ", 0, 200);
    int eastWest = readInt("Vehicles in East-West direction (0-200): ", 0, 200);

    TrafficPhase phase = analyzer.choosePhase(northSouth, eastWest);
    int greenTime = analyzer.calculateGreenTime(
        northSouth > eastWest ? northSouth : eastWest);

    cout << "Selected phase: ";
    if (phase == NORTH_SOUTH_PHASE)
    {
        cout << "North-South\n";
    }
    else
    {
        cout << "East-West\n";
    }
    cout << "Calculated green time: " << greenTime << " seconds\n";

    controller.activatePhase(phase);
    controller.displayStatusLine();
    controller.finishPhase();
    cout << "After finishing phase:\n";
    controller.displayReport();
}

void testModule3()
{
    printHeader("MODULE 3 - VEHICLE DETECTION");

    VehicleDetector detector;
    detector.chooseMode();

    const char* laneNames[4] = {"North", "South", "East", "West"};
    for (int i = 0; i < 4; i++)
    {
        int count = detector.detectVehicles(laneNames[i]);
        cout << laneNames[i] << " vehicles detected: " << count << '\n';

        int emergencies = detector.detectEmergencies(laneNames[i], count);
        cout << "Emergency vehicles detected: " << emergencies << '\n';

        int used[VehicleDetector::MAX_EMERGENCY_PER_LANE] = {0};
        for (int j = 0; j < emergencies; j++)
        {
            used[j] = detector.detectEmergencyPosition(count, used, j);
            cout << "Emergency vehicle position: " << used[j] << '\n';
        }
    }
}

void testModule4()
{
    printHeader("MODULE 4 - EMERGENCY ROUTING");

    EmergencyManager manager;
    int count = readInt("How many emergency vehicles to add (1-5)? ", 1, 5);

    for (int i = 1; i <= count; i++)
    {
        int id = readInt("Enter emergency vehicle ID: ", 1, 10000);
        int priority = readInt("Enter priority (1-100): ", 1, 100);
        Vehicle emergency(id, EMERGENCY, i, priority);
        manager.insertEmergency(emergency);
    }

    manager.displayPriorityQueue();
    Vehicle next;
    if (manager.extractHighestPriority(next))
    {
        cout << "Next emergency to dispatch: " << next << '\n';
    }
    manager.displayReport();

    GreenCorridor corridor;
    corridor.setupDemoNetwork();
    corridor.displayNetwork();
    int source = readInt("Enter source intersection (0-9): ", 0, 9);
    int destination = readInt("Enter destination intersection (0-9): ", 0, 9);
    corridor.displayRoute(source, destination);
}

void testModule5()
{
    printHeader("MODULE 5 - ANALYTICS & REPORTS");

    PerformanceAnalytics analytics;
    int count = readInt("How many service records to add (1-10)? ", 1, 10);

    for (int i = 1; i <= count; i++)
    {
        cout << "Record " << i << '\n';
        int id = readInt("Vehicle ID: ", 1, 10000);
        int lane = readInt("Lane code (0=Corridor, 1=North, 2=South, 3=East, 4=West): ", 0, 4);
        int serviceTime = readInt("Service time in seconds: ", 0, 100000);
        int waitTime = readInt("Waiting time in seconds: ", 0, 100000);
        bool emergency = readYesNo("Was it an emergency vehicle?");
        analytics.addRecord(id, lane, serviceTime, waitTime, emergency);
    }

    analytics.displayServiceHistory();
    analytics.displayReport();
    analytics.displaySortedHistory();

    int searchId = readInt("Enter vehicle ID to search: ", 1, 10000);
    analytics.searchAndDisplayVehicle(searchId);
}

void testModule6()
{
    printHeader("MODULE 6 - FULL SIMULATION ENGINE");
    cout << "Module 6 coordinates all modules, so this option runs the integrated simulation.\n";
    TrafficSimulation simulation;
    simulation.run();
}

int main()
{
    while (true)
    {
        cout << "1. Vehicle & Lane Management\n";
        cout << "2. Signal Control & Adaptive Timing\n";
        cout << "3. Vehicle Detection\n";
        cout << "4. Emergency Routing\n";
        cout << "5. Analytics & Reports\n";
        cout << "6. Simulation Engine (integrated simulation)\n";
        cout << "0. Exit\n";

        int choice = readInt("Choose module: ", 0, 6);
        if (choice == 0)
        {
            cout << "Goodbye.\n";
            break;
        }
        else if (choice == 1)
        {
            testModule1();
        }
        else if (choice == 2)
        {
            testModule2();
        }
        else if (choice == 3)
        {
            testModule3();
        }
        else if (choice == 4)
        {
            testModule4();
        }
        else if (choice == 5)
        {
            testModule5();
        }
        else if (choice == 6)
        {
            testModule6();
        }

        cout << '\n';
    }

    return 0;
}
