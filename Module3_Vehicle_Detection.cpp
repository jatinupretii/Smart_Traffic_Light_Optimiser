// STUDENT 3 — VEHICLE DETECTION & INPUT
// Manual/random sensor simulation and emergency-position detection.
#include "Common.h"

bool readYesNo(const char* prompt) 
{
    char answer;

    while (true) 
    {
        cout << prompt << " (y/n): ";

        if (cin >> answer) 
        {
            if (answer == 'y' || answer == 'Y') return true;
            if (answer == 'n' || answer == 'N') return false;
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
            strcpy(prompt, "Enter vehicles for ");
            strcat(prompt, laneName);
            strcat(prompt, ": ");
            return readInt(prompt, 0, LANE_CAPACITY);
        }

        int count = nextInt(5, 40);
        cout << "[SENSOR] " << laneName << " lane: "
             << count << " vehicles detected\n";
        return count;
    }

    // Sensor 2: detects how many generic emergency vehicles are in a lane.
    int detectEmergencies(const char* laneName, int vehicleCount) 
    {
        if (vehicleCount == 0) 
        {
            return 0;
        }

        int limit = vehicleCount < MAX_EMERGENCY_PER_LANE
                        ? vehicleCount
                        : MAX_EMERGENCY_PER_LANE;

        if (mode == 1) 
        {
            char prompt[70];
            strcpy(prompt, "How many emergency vehicles in ");
            strcat(prompt, laneName);
            strcat(prompt, "? ");
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
    int detectEmergencyPosition(int vehicleCount,
                                const int used[], int usedCount) 
    {
        while (true) 
        {
            int position;

            if (mode == 1) 
            {
                char prompt[60];
                strcpy(prompt, "Enter emergency vehicle ID (1 to ");
                char number[12];
                int n = vehicleCount;
                int digits = 0;
                char reversed[12];

                do {
                    reversed[digits++] = static_cast<char>('0' + n % 10);
                    n /= 10;
                } while (n > 0);

                for (int i = 0; i < digits; i++) 
                {
                    number[i] = reversed[digits - 1 - i];
                }

                number[digits] = '\0';
                strcat(prompt, number);
                strcat(prompt, "): ");
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
