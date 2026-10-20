//Module 3:: Vehicle Detection

#include "Common.h"

bool readYesNo(const char* prompt)
{
    char answer;

    while (true)
    {
        cout << prompt << " (y/n): ";

        if (cin >> answer)
        {
            if (answer == 'y' || answer == 'Y')
            {
                return true;
            }

            if (answer == 'n' || answer == 'N')
            {
                return false;
            }
        }
        else if (cin.eof())
        {
            cout << "\nInput ended. Exiting program.\n";
            exit(0);
        }
        else
        {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }

        cout << "Please answer with y or n.\n";
    }
}

// A console program has no real sensor, so detection is simulated:
//   mode 1 = the operator types what the sensor reads
//   mode 2 = a seeded random generator acts as the sensor
class VehicleDetector
{
private:
    int mode;                  // 1 = manual, 2 = random
    unsigned long seed;

    int nextInt(int low, int high)
    {
        seed = (seed * 1103515245UL + 12345UL) & 0x7fffffffUL;
        unsigned long range = static_cast<unsigned long>(high - low + 1);
        return low + static_cast<int>((seed >> 8) % range);
    }

public:
    static const int MAX_EMERGENCY_PER_LANE = 5;

    VehicleDetector()
    {
        mode = 1;
        seed = 2026;
    }

    void chooseMode()
    {
        cout << "Detection mode:\n";
        cout << "  1 = Manual entry (operator types what the sensor reads)\n";
        cout << "  2 = Random sensor simulation\n";
        mode = readInt("Choose mode: ", 1, 2);
        cout << '\n';
    }

    // Sensor 1: counts the vehicles waiting in a lane.
    int detectVehicles(const char* laneName)
    {
        if (mode == 1)
        {
            char prompt[60];
            snprintf(prompt, sizeof(prompt), "Enter vehicles for %s: ", laneName);
            return readInt(prompt, 0, LANE_CAPACITY);
        }

        int count = nextInt(5, 40);
        cout << "[SENSOR] " << laneName << " lane: " << count << " vehicles detected\n";
        return count;
    }

    // Sensor 2: detects how many emergency vehicles are in a lane.
    int detectEmergencies(const char* laneName, int vehicleCount)
    {
        if (vehicleCount == 0)
        {
            return 0;
        }

        int limit = vehicleCount;

        if (limit > MAX_EMERGENCY_PER_LANE)
        {
            limit = MAX_EMERGENCY_PER_LANE;
        }

        if (mode == 1)
        {
            char prompt[70];
            snprintf(prompt, sizeof(prompt), "How many emergency vehicles in %s? ", laneName);
            return readInt(prompt, 0, limit);
        }

        int found = (nextInt(1, 4) == 1) ? 1 : 0;

        if (found > 0)
        {
            cout << "[SENSOR] Siren detected in " << laneName << " lane\n";
        }

        return found;
    }

    // Position (1 = front of queue) of the emergency vehicle in its lane.
    int detectEmergencyPosition(int vehicleCount, const int used[], int usedCount)
    {
        while (true)
        {
            int position;

            if (mode == 1)
            {
                char prompt[60];
                snprintf(prompt, sizeof(prompt), "Enter emergency vehicle ID (1 to %d): ", vehicleCount);
                position = readInt(prompt, 1, vehicleCount);
            }
            else
            {
                position = nextInt(1, vehicleCount);
            }

            bool alreadyUsed = false;

            for (int i = 0; i < usedCount; i++)
            {
                if (used[i] == position)
                {
                    alreadyUsed = true;
                }
            }

            if (!alreadyUsed)
            {
                return position;
            }

            if (mode == 1)
            {
                cout << "That vehicle is already marked as emergency.\n";
            }
        }
    }
};