#include "Common.h"

enum VehicleType
{
    UNKNOWN = -1,
    CAR = 0,
    BUS = 1,
    MOTORBIKE = 2,
    TRUCK = 3,
    EMERGENCY = 4
};

class Vehicle
{
private:
    int vehicleId;
    VehicleType type;
    int arrivalTime;
    int priority;
    static int createdCount;

public:
    static bool isEmergencyType(VehicleType type)
    {
        if(type == EMERGENCY)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    static int defaultPriority(VehicleType type)
    {
        if (type == EMERGENCY)
        {
            return 100;
        }

        return 0;
    }

    static const char* typeName(VehicleType type)
    {
        switch (type)
        {
            case UNKNOWN:
                return "Unknown";
            case CAR: 
                return "Car";
            case BUS: 
                return "Bus";
            case MOTORBIKE: 
                return "Motorbike";
            case TRUCK: 
                return "Truck";
            case EMERGENCY: 
                return "Emergency Vehicle";
            default:
                 return "Unknown";
        } 
    }

    static int getCreatedCount()
    {
        return createdCount;
    }

    Vehicle()
    {
        vehicleId = 0;
        type = UNKNOWN;
        arrivalTime = 0;
        priority = 0;
    }

    Vehicle(int vehicleId, VehicleType type, int arrival, int emergencyPriority = 0)
    {
        this->vehicleId = vehicleId;
        this->type = type;
        arrivalTime = arrival;

       if (type == EMERGENCY)
        {
            if (emergencyPriority > 0)
            {
                priority = emergencyPriority;
            }
            else
            {
                priority = 100;
            }
        }
        else
        {
            priority = 0;
        }
        createdCount++;
    }

    int getId() const
    {
        return vehicleId;
    }

    VehicleType getType() const
    {
        return type;
    }

    int getArrivalTime() const
    {
        return arrivalTime;
    }

    int getPriority() const
    {
        return priority;
    }

    bool isEmergency() const
    {
        return isEmergencyType(type);
    }

    friend ostream& operator<<(ostream& out, const Vehicle& vehicle);
};

int Vehicle::createdCount = 0;

ostream& operator<<(ostream& out, const Vehicle& vehicle)
{
    out << Vehicle::typeName(vehicle.type) << " " << vehicle.vehicleId << " (arrived " << vehicle.arrivalTime << "s";

    if (vehicle.isEmergency())
    {
        out << ", priority " << vehicle.priority;
    }

    out << ")";
    return out;
}

class Lane : public Reportable
{
private:
    Vehicle vehicles[LANE_CAPACITY];
    int frontIndex;
    int rearIndex;
    int vehicleCount;
    int totalArrived;
    int totalServed;
    int totalRejected;
    int totalExtracted;
    char laneName[20];

    void initCounters()
    {
        frontIndex = -1;
        rearIndex = -1;
        vehicleCount = 0;
        totalArrived = 0;
        totalServed = 0;
        totalRejected = 0;
        totalExtracted = 0;
    }

public:
    Lane()
    {
        initCounters();
        strcpy(laneName, "Unknown");
    }

    Lane(const char* name)
    {
        initCounters();

        if (name == NULL)
        {
            strcpy(laneName, "Unknown");
        }
        else
        {
            strncpy(laneName, name, sizeof(laneName) - 1);
            laneName[sizeof(laneName) - 1] = '\0';
        }
    }

    bool isEmpty() const
    {
        if(frontIndex==-1)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    bool isFull() const
    {
        if(vehicleCount == LANE_CAPACITY)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    int getCount() const
    {
        return vehicleCount;
    }

    const char* getName() const
    {
        return laneName;
    }

    bool addVehicle(const Vehicle& vehicle)
    {
        if(isFull())
        {
            totalRejected++;
            return false;
        }
        if(frontIndex == -1)
        {
            frontIndex = 0;
            rearIndex = 0;
        }
        else
        {
            rearIndex = (rearIndex + 1) % LANE_CAPACITY;
        }
        vehicles[rearIndex] = vehicle;
        vehicleCount++;
        totalArrived++;
        return true;
    }

    bool serveVehicle(Vehicle& servedVehicle)
    {
        if (isEmpty())
        {
            return false;
        }

        servedVehicle = vehicles[frontIndex];
        vehicleCount--;
        totalServed++;

        if (vehicleCount == 0)
        {
            frontIndex = -1;
            rearIndex = -1;
        }
        else
        {
            frontIndex = (frontIndex + 1) % LANE_CAPACITY;
        }

        return true;
    }

    bool peekVehicle(Vehicle& vehicle) const
    {
        if (isEmpty())
        {
            return false;
        }

        vehicle = vehicles[frontIndex];
        return true;
    }

    bool extractVehicleById(int vehicleId, Vehicle& removedVehicle)
    {
        if (isEmpty())
        {
            return false;
        }

        int position = -1;

        for (int i = 0; i < vehicleCount; i++)
        {
            int index = (frontIndex + i) % LANE_CAPACITY;

            if (vehicles[index].getId() == vehicleId)
            {
                position = i;
                break;
            }
        }

        if (position == -1)
        {
            return false;
        }

        int removeIndex = (frontIndex + position) % LANE_CAPACITY;
        removedVehicle = vehicles[removeIndex];

        for (int i = position; i < vehicleCount - 1; i++)
        {
            int currentIndex = (frontIndex + i) % LANE_CAPACITY;
            int nextIndex = (frontIndex + i + 1) % LANE_CAPACITY;
            vehicles[currentIndex] = vehicles[nextIndex];
        }

        vehicleCount--;
        totalExtracted++;

        if (vehicleCount == 0)
        {
            frontIndex = -1;
            rearIndex = -1;
        }
        else
        {
            rearIndex = (rearIndex - 1 + LANE_CAPACITY) % LANE_CAPACITY;
        }

        return true;
    }

    void displayStatusBar() const
    {
        const int BAR_WIDTH = 20;
        int filled = vehicleCount * BAR_WIDTH / LANE_CAPACITY;

        cout << left << setw(7) << laneName << " [";

        for (int i = 0; i < BAR_WIDTH; i++)
        {
            if (i < filled)
            {
                cout << '#';
            }
            else
            {
                cout << '.';
            }
        }
        cout << "] " << vehicleCount << "/" << LANE_CAPACITY << '\n';
    }

    void displayQueue() const
    {
        cout << laneName << " queue: ";

        if (isEmpty())
        {
            cout << "Empty\n";
            return;
        }

        for (int i = 0; i < vehicleCount; i++)
        {
            int index = (frontIndex + i) % LANE_CAPACITY;
            cout << vehicles[index].getId();

            if (i < vehicleCount - 1)
            {
                cout << " -> ";
            }
        }

        cout << '\n';
    }

    void displayReport() const
    {
        cout << left << setw(7) << laneName << right << " waiting: " << setw(3) << vehicleCount << " | arrived: " << setw(3) << totalArrived << " | served: " << setw(3) << totalServed << " | extracted: " << setw(3) << totalExtracted << " | rejected: " << totalRejected << '\n';
    }

    const char* moduleName() const
    {
        return "Module 1 - Lane Management";
    }
};
