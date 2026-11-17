// Smart Traffic Light Optimiser//
//  Vehicle input: detections.txt -> traffic simulation

#include <iostream>
#include <iomanip>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
using namespace std;

const int SECONDS_PER_VEHICLE = 3;
const int YELLOW_TIME = 3;
const int ALL_RED_TIME = 2;

// Green corridor lane: emergency vehicles wait in their own lane, so they never queue with
// ordinary traffic. The junction is held red only while one of them crosses it:
// SECONDS_PER_VEHICLE to cross the stop line + ALL_RED_TIME to clear the junction.
// The rest of its route is a green hold at the next junctions (not simulated here).
const int EMERGENCY_CROSSING_TIME = SECONDS_PER_VEHICLE + ALL_RED_TIME;
const int LANE_CAPACITY = 100;
const int MAX_EMERGENCIES = 100;
const int MAX_INTERSECTIONS = 10;

// Text input file; can be changed from the command line.
const char *inputFileName = "detections.txt";

template <typename T>
void swapValues(T &a, T &b)
{
    T temp = a;
    a = b;
    b = temp;
}

void printHeader(const char *title)
{
    cout << "\n============================================================\n";
    cout << "  " << title << '\n';
    cout << "============================================================\n";
}

// Swallows all console text (used by batch mode so only the JSON report is printed).
class NullBuffer : public streambuf
{
public:
    int overflow(int c) { return c; }
};

int readInt(const char *prompt, int minValue, int maxValue)
{
    int value;
    while (true)
    {
        cout << prompt;
        if (cin >> value)
        {
            if (value >= minValue && value <= maxValue)
                return value;
        }
        else if (cin.eof())
        {
            cout << "\nInput ended. Exiting program.\n";
            exit(0);
        }
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Invalid input. Enter a number from " << minValue
             << " to " << maxValue << ".\n";
    }
}

class Reportable
{
public:
    virtual ~Reportable() {}
    virtual void displayReport() const = 0;
    virtual const char *moduleName() const = 0;
};

// Module 1:: Vehicle & Lane Management

enum VehicleType
{
    UNKNOWN = -1,
    CAR = 0,
    BUS = 1,
    MOTORBIKE = 2,
    TRUCK = 3,
    AMBULANCE = 4,
    FIRE_TRUCK = 5,
    POLICE_CAR = 6
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
        if (type == AMBULANCE || type == FIRE_TRUCK || type == POLICE_CAR)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    // Higher number = served first by the emergency heap.
    // Order used here: ambulance (life at risk) > fire truck > police car.
    static int defaultPriority(VehicleType type)
    {
        if (type == AMBULANCE)
        {
            return 100;
        }

        if (type == FIRE_TRUCK)
        {
            return 90;
        }

        if (type == POLICE_CAR)
        {
            return 70;
        }

        return 0;
    }

    static const char *typeName(VehicleType type)
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
        case AMBULANCE:
            return "Ambulance";
        case FIRE_TRUCK:
            return "Fire Truck";
        case POLICE_CAR:
            return "Police Car";
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

        if (isEmergencyType(type))
        {
            if (emergencyPriority > 0)
            {
                priority = emergencyPriority;
            }
            else
            {
                priority = defaultPriority(type);
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

    friend ostream &operator<<(ostream &out, const Vehicle &vehicle);
};

int Vehicle::createdCount = 0;

ostream &operator<<(ostream &out, const Vehicle &vehicle)
{
    out << Vehicle::typeName(vehicle.type) << " " << vehicle.vehicleId << " (arrived " << vehicle.arrivalTime << "s";

    if (vehicle.isEmergency())
    {
        out << ", priority " << vehicle.priority;
    }

    out << ")";
    return out;
}

// Where each kind of emergency vehicle is heading (node numbers of GreenCorridor).
// DEMO ASSUMPTION: an ambulance goes to the hospital; fire trucks and police cars go to
// a fixed incident site. Change the numbers to match your map.
int emergencyDestination(VehicleType type)
{
    if (type == AMBULANCE)
    {
        return 0; // City Hospital
    }

    if (type == FIRE_TRUCK)
    {
        return 9; // Stadium Gate (assumed incident site)
    }

    return 7; // Civil Lines (assumed incident site)
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

    Lane(const char *name)
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
        if (frontIndex == -1)
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
        if (vehicleCount == LANE_CAPACITY)
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

    const char *getName() const
    {
        return laneName;
    }

    // Enqueue
    bool addVehicle(const Vehicle &vehicle)
    {
        if (isFull())
        {
            totalRejected++;
            return false;
        }
        if (frontIndex == -1)
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

    // Dequeue
    bool serveVehicle(Vehicle &servedVehicle)
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

    // Peek
    bool peekVehicle(Vehicle &vehicle) const
    {
        if (isEmpty())
        {
            return false;
        }

        vehicle = vehicles[frontIndex];
        return true;
    }

    //(used for emergency vehicles).
    bool extractVehicleById(int vehicleId, Vehicle &removedVehicle)
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

        cout << left << setw(7) << laneName << right << " [";

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

    const char *moduleName() const
    {
        return "Module 1 - Lane Management";
    }
};

// Module 2:: Signal Control & Adaptive Timing

enum SignalState
{
    RED,
    YELLOW,
    GREEN
};

const char *signalStateText(SignalState state)
{
    if (state == RED)
    {
        return "RED";
    }
    if (state == YELLOW)
    {
        return "YELLOW";
    }
    return "GREEN";
}

class TrafficSignal
{
private:
    char signalName[30];
    SignalState currentState;
    int stateChangeCount;

public:
    TrafficSignal()
    {
        strcpy(signalName, "Unknown Signal");
        currentState = RED;
        stateChangeCount = 0;
    }

    TrafficSignal(const char *name)
    {
        strncpy(signalName, name, sizeof(signalName) - 1);
        signalName[sizeof(signalName) - 1] = '\0';
        currentState = RED;
        stateChangeCount = 0;
    }

    SignalState getState() const
    {
        return currentState;
    }

    const char *getName() const
    {
        return signalName;
    }

    const char *getStateText() const
    {
        return signalStateText(currentState);
    }

    int getStateChangeCount() const
    {
        return stateChangeCount;
    }

    // Only RED -> GREEN -> YELLOW -> RED is allowed.
    bool changeState(SignalState newState)
    {
        if (newState == currentState)
        {
            return true;
        }

        bool valid = (currentState == RED && newState == GREEN) || (currentState == GREEN && newState == YELLOW) || (currentState == YELLOW && newState == RED);

        if (!valid)
        {
            cout << "Invalid transition for " << signalName << ": " << signalStateText(currentState) << " -> " << signalStateText(newState) << " is not allowed.\n";
            return false;
        }

        currentState = newState;
        stateChangeCount++;
        return true;
    }
};

// Linked list that records every signal change.
class SignalEventLog
{
private:
    struct Node
    {
        int eventNumber;
        char signalName[30];
        SignalState oldState;
        SignalState newState;
        Node *next;

        Node(int number, const char *name, SignalState oldValue, SignalState newValue)
        {
            eventNumber = number;
            strncpy(signalName, name, sizeof(signalName) - 1);
            signalName[sizeof(signalName) - 1] = '\0';
            oldState = oldValue;
            newState = newValue;
            next = NULL;
        }
    };

    Node *head;
    Node *tail;
    int eventCount;

public:
    SignalEventLog() : head(NULL), tail(NULL), eventCount(0)
    {
    }

    SignalEventLog(const SignalEventLog &) = delete;
    SignalEventLog &operator=(const SignalEventLog &) = delete;

    ~SignalEventLog()
    {
        Node *current = head;

        while (current != NULL)
        {
            Node *nextNode = current->next;
            delete current;
            current = nextNode;
        }
    }

    void addEvent(const char *name, SignalState oldState, SignalState newState)
    {
        Node *newNode = new Node(eventCount + 1, name, oldState, newState);

        if (head == NULL)
        {
            head = newNode;
            tail = newNode;
        }
        else
        {
            tail->next = newNode;
            tail = newNode;
        }

        eventCount++;
    }

    int size() const
    {
        return eventCount;
    }

    void display() const
    {
        cout << "\nSignal Event Log (linked list):\n";

        if (head == NULL)
        {
            cout << "No signal transitions recorded.\n";
            return;
        }

        const Node *current = head;

        while (current != NULL)
        {
            cout << "  #" << current->eventNumber << "  " << current->signalName << ": " << signalStateText(current->oldState) << " -> " << signalStateText(current->newState) << '\n';
            current = current->next;
        }
    }
};

enum TrafficPhase
{
    NORTH_SOUTH_PHASE,
    EAST_WEST_PHASE
};

class PhaseController : public Reportable
{
private:
    TrafficSignal northSouthSignal;
    TrafficSignal eastWestSignal;
    TrafficPhase activePhase;
    int phaseSwitches;
    int safetyViolations;
    SignalEventLog eventLog;

    void changeAndLog(TrafficSignal &signal, SignalState newState)
    {
        SignalState oldState = signal.getState();

        if (oldState == newState)
        {
            return;
        }

        if (signal.changeState(newState))
        {
            eventLog.addEvent(signal.getName(), oldState, newState);
        }
    }

    void moveToRed(TrafficSignal &signal)
    {
        if (signal.getState() == GREEN)
        {
            changeAndLog(signal, YELLOW);
        }

        if (signal.getState() == YELLOW)
        {
            changeAndLog(signal, RED);
        }
    }

public:
    PhaseController() : northSouthSignal("North-South Signal"), eastWestSignal("East-West Signal")
    {
        activePhase = NORTH_SOUTH_PHASE;
        phaseSwitches = 0;
        safetyViolations = 0;
    }

    static int transitionTime()
    {
        return YELLOW_TIME + ALL_RED_TIME;
    }

    void allRed()
    {
        moveToRed(northSouthSignal);
        moveToRed(eastWestSignal);
    }

    void activatePhase(TrafficPhase phase)
    {
        allRed();

        if (phase != activePhase)
        {
            phaseSwitches++;
        }

        activePhase = phase;

        if (phase == NORTH_SOUTH_PHASE)
        {
            changeAndLog(northSouthSignal, GREEN);
        }
        else
        {
            changeAndLog(eastWestSignal, GREEN);
        }

        if (!isSafe())
        {
            safetyViolations++;
        }
    }

    void finishPhase()
    {
        allRed();
    }

    bool isSafe() const
    {
        return !(northSouthSignal.getState() == GREEN && eastWestSignal.getState() == GREEN);
    }

    int getPhaseSwitches() const
    {
        return phaseSwitches;
    }

    void displayStatusLine() const
    {
        cout << "Signals -> North-South: " << northSouthSignal.getStateText() << " | East-West: " << eastWestSignal.getStateText() << '\n';
    }

    void displayReport() const
    {
        displayStatusLine();
        cout << "North-South state changes: " << northSouthSignal.getStateChangeCount() << '\n';
        cout << "East-West state changes: " << eastWestSignal.getStateChangeCount() << '\n';
        cout << "Phase switches: " << phaseSwitches << '\n';
        cout << "Safety violations (both green): " << safetyViolations << '\n';
        cout << "Recorded signal events: " << eventLog.size() << '\n';
        eventLog.display();
    }

    const char *moduleName() const
    {
        return "Module 2 - Signal Phase Control";
    }
};

class TrafficAnalyzer : public Reportable
{
private:
    int secondsPerVehicle;
    int minGreenTime;
    int maxGreenTime;

public:
    TrafficAnalyzer()
    {
        secondsPerVehicle = SECONDS_PER_VEHICLE; // 3 seconds (see Common.h)
        minGreenTime = 10;
        maxGreenTime = 60;
    }

    int getSecondsPerVehicle() const
    {
        return secondsPerVehicle;
    }

    void setGreenLimits(int minTime, int maxTime)
    {
        if (minTime > 0 && maxTime >= minTime)
        {
            minGreenTime = minTime;
            maxGreenTime = maxTime;
        }
    }

    // Green time = vehicles x seconds per vehicle, kept between min and max.
    int calculateGreenTime(int vehicleCount) const
    {
        if (vehicleCount < 0)
        {
            vehicleCount = 0;
        }

        int greenTime = vehicleCount * secondsPerVehicle;

        if (greenTime < minGreenTime)
        {
            greenTime = minGreenTime;
        }

        if (greenTime > maxGreenTime)
        {
            greenTime = maxGreenTime;
        }

        return greenTime;
    }

    // The direction with more vehicles gets the green first.
    TrafficPhase choosePhase(int northSouthCount, int eastWestCount) const
    {
        if (northSouthCount >= eastWestCount)
        {
            return NORTH_SOUTH_PHASE;
        }

        return EAST_WEST_PHASE;
    }

    void displayReport() const
    {
        cout << "\nAdaptive Timing Report\n";
        cout << "Seconds per vehicle: " << secondsPerVehicle << '\n';
        cout << "Minimum green time: " << minGreenTime << " seconds\n";
        cout << "Maximum green time: " << maxGreenTime << " seconds\n";
    }

    const char *moduleName() const
    {
        return "Module 2 - Adaptive Traffic Timing";
    }
};

// Module 3:: Vehicle Detection

bool readYesNo(const char *prompt)
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

// A console program has no real camera, so detection has three modes:
//   mode 1 = the operator types what the sensor reads
//   mode 2 = a seeded random generator acts as the sensor
//   mode 3 = file input: reads vehicle data from detections.txt
class VehicleDetector
{
private:
    int mode; // 1 = manual, 2 = random, 3 = text file
    unsigned long seed;

    // File-input data: each lane lists its vehicles from the stop line backwards
    int fileInputCount[4];
    VehicleType fileInputType[4][LANE_CAPACITY];
    VehicleType fileInputEmergency[4][LANE_CAPACITY]; // UNKNOWN = ordinary vehicle

    int nextInt(int low, int high)
    {
        seed = (seed * 1103515245UL + 12345UL) & 0x7fffffffUL;
        unsigned long range = static_cast<unsigned long>(high - low + 1);
        return low + static_cast<int>((seed >> 8) % range);
    }

    // Case-insensitive text comparison
    static bool sameWord(const char *a, const char *b)
    {
        while (*a != '\0' && *b != '\0')
        {
            char x = *a;
            char y = *b;

            if (x >= 'A' && x <= 'Z')
            {
                x = x - 'A' + 'a';
            }

            if (y >= 'A' && y <= 'Z')
            {
                y = y - 'A' + 'a';
            }

            if (x != y)
            {
                return false;
            }

            a++;
            b++;
        }

        return *a == *b;
    }

    // LINEAR SEARCH: lane name -> index 0..3 (-1 if unknown)
    static int laneIndex(const char *name)
    {
        const char *names[4] = {"North", "South", "East", "West"};

        for (int i = 0; i < 4; i++)
        {
            if (sameWord(name, names[i]))
            {
                return i;
            }
        }

        return -1;
    }

    int emergencyCountInLane(int lane) const
    {
        int total = 0;

        for (int i = 0; i < fileInputCount[lane]; i++)
        {
            if (fileInputEmergency[lane][i] != UNKNOWN)
            {
                total++;
            }
        }

        return total;
    }

    // Line format:  North: car car bus ambulance motorbike   (front of queue first)
    bool loadInputFile(const char *fileName)
    {
        FILE *file = fopen(fileName, "r");

        if (file == NULL)
        {
            return false;
        }

        for (int lane = 0; lane < 4; lane++)
        {
            fileInputCount[lane] = 0;
        }

        char line[4096];

        while (fgets(line, sizeof(line), file) != NULL)
        {
            int pos = 0;
            int used = 0;

            while (line[pos] == ' ' || line[pos] == '\t')
            {
                pos++;
            }

            if (line[pos] == '#' || line[pos] == '\n' || line[pos] == '\r' || line[pos] == '\0')
            {
                continue;
            }

            char laneWord[20];

            if (sscanf(line + pos, "%19[^: \t]%n", laneWord, &used) != 1)
            {
                continue;
            }

            pos += used;

            while (line[pos] == ':' || line[pos] == ' ' || line[pos] == '\t')
            {
                pos++;
            }

            int lane = laneIndex(laneWord);

            if (lane < 0)
            {
                cout << "[FILE INPUT] Unknown lane '" << laneWord << "' ignored.\n";
                continue;
            }

            char word[30];

            while (sscanf(line + pos, "%29s%n", word, &used) == 1)
            {
                pos += used;

                if (fileInputCount[lane] >= LANE_CAPACITY)
                {
                    cout << "[FILE INPUT] " << laneWord << " lane is over capacity; extra vehicles ignored.\n";
                    break;
                }

                VehicleType type = CAR;
                VehicleType emergencyKind = UNKNOWN;

                if (sameWord(word, "car"))
                {
                    type = CAR;
                }
                else if (sameWord(word, "bus"))
                {
                    type = BUS;
                }
                else if (sameWord(word, "motorbike") || sameWord(word, "motorcycle"))
                {
                    type = MOTORBIKE;
                }
                else if (sameWord(word, "truck"))
                {
                    type = TRUCK;
                }
                else if (sameWord(word, "ambulance"))
                {
                    emergencyKind = AMBULANCE; // moved to the emergency heap later
                }
                else if (sameWord(word, "firetruck") || sameWord(word, "fire_truck") || sameWord(word, "fire-truck"))
                {
                    emergencyKind = FIRE_TRUCK;
                }
                else if (sameWord(word, "police") || sameWord(word, "policecar") ||
                         sameWord(word, "police_car") || sameWord(word, "police-car"))
                {
                    emergencyKind = POLICE_CAR;
                }
                else
                {
                    cout << "[FILE INPUT] Unknown vehicle '" << word << "' ignored.\n";
                    continue;
                }

                int index = fileInputCount[lane];
                fileInputType[lane][index] = type;
                fileInputEmergency[lane][index] = emergencyKind;
                fileInputCount[lane]++;
            }
        }

        fclose(file);
        return true;
    }

    void showInputSummary() const
    {
        const char *names[4] = {"North", "South", "East", "West"};

        cout << "\n--- FILE INPUT SUMMARY (" << inputFileName << ") ---\n";

        for (int lane = 0; lane < 4; lane++)
        {
            int cars = 0;
            int buses = 0;
            int bikes = 0;
            int trucks = 0;
            int ambulances = 0;
            int fireTrucks = 0;
            int policeCars = 0;

            for (int i = 0; i < fileInputCount[lane]; i++)
            {
                if (fileInputEmergency[lane][i] == AMBULANCE)
                {
                    ambulances++;
                }
                else if (fileInputEmergency[lane][i] == FIRE_TRUCK)
                {
                    fireTrucks++;
                }
                else if (fileInputEmergency[lane][i] == POLICE_CAR)
                {
                    policeCars++;
                }
                else if (fileInputType[lane][i] == CAR)
                {
                    cars++;
                }
                else if (fileInputType[lane][i] == BUS)
                {
                    buses++;
                }
                else if (fileInputType[lane][i] == MOTORBIKE)
                {
                    bikes++;
                }
                else
                {
                    trucks++;
                }
            }

            cout << left << setw(6) << names[lane] << right << ": " << setw(3) << fileInputCount[lane]
                 << " vehicles | car " << cars << ", bus " << buses << ", motorbike " << bikes
                 << ", truck " << trucks << " | ambulance " << ambulances
                 << ", fire truck " << fireTrucks << ", police " << policeCars << '\n';
        }
    }

public:
    static const int MAX_EMERGENCY_PER_LANE = 5;

    VehicleDetector()
    {
        mode = 1;
        seed = 2026;

        for (int lane = 0; lane < 4; lane++)
        {
            fileInputCount[lane] = 0;
        }
    }

    void chooseMode()
    {
        cout << "Detection mode:\n";
        cout << "  1 = Manual entry (operator types what the sensor reads)\n";
        cout << "  2 = Random sensor simulation\n";
        cout << "  3 = File input (reads " << inputFileName << ")\n";
        mode = readInt("Choose mode: ", 1, 3);

        if (mode == 3)
        {
            if (loadInputFile(inputFileName))
            {
                showInputSummary();
            }
            else
            {
                cout << "Could not read " << inputFileName << ". Check that the input file exists.\n";
                cout << "Falling back to manual entry.\n";
                mode = 1;
            }
        }

        cout << '\n';
    }

    // Non-interactive: load vehicle data from this text file (batch mode / front end).
    bool useInputFile(const char *fileName)
    {
        if (!loadInputFile(fileName))
        {
            return false;
        }

        mode = 3;
        return true;
    }

    // Sensor 1: counts the vehicles waiting in a lane.
    int detectVehicles(const char *laneName)
    {
        if (mode == 3)
        {
            int lane = laneIndex(laneName);
            int count = (lane >= 0) ? fileInputCount[lane] : 0;
            cout << "[FILE INPUT] " << laneName << " lane: " << count << " vehicles detected\n";
            return count;
        }

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

    // Type of the vehicle at queue position 'index' (0 = front of the lane).
    // File-input mode returns the vehicle type stored in the text file; other modes cycle through types.
    VehicleType vehicleTypeAt(const char *laneName, int index) const
    {
        if (mode == 3)
        {
            int lane = laneIndex(laneName);

            if (lane >= 0 && index < fileInputCount[lane])
            {
                return fileInputType[lane][index];
            }
        }

        return static_cast<VehicleType>(index % 4);
    }

    // Sensor 2: detects how many emergency vehicles are in a lane.
    int detectEmergencies(const char *laneName, int vehicleCount)
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

        if (mode == 3)
        {
            int lane = laneIndex(laneName);
            int found = (lane >= 0) ? emergencyCountInLane(lane) : 0;

            if (found > limit)
            {
                found = limit;
            }

            if (found > 0)
            {
                cout << "[FILE INPUT] " << found << " emergency vehicle(s) detected in " << laneName << " lane\n";
            }

            return found;
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

    // Which kind of emergency vehicle is at this queue position (1 = front)?
    VehicleType detectEmergencyKind(const char *laneName, int position)
    {
        if (mode == 3)
        {
            int lane = laneIndex(laneName);

            if (lane >= 0 && position >= 1 && position <= fileInputCount[lane] &&
                fileInputEmergency[lane][position - 1] != UNKNOWN)
            {
                return fileInputEmergency[lane][position - 1];
            }

            return AMBULANCE;
        }

        if (mode == 1)
        {
            cout << "  1 = Ambulance, 2 = Fire truck, 3 = Police car\n";
            int choice = readInt("Type of this emergency vehicle: ", 1, 3);
            return static_cast<VehicleType>(AMBULANCE + choice - 1);
        }

        VehicleType kind = static_cast<VehicleType>(AMBULANCE + nextInt(0, 2));
        cout << "[SENSOR] " << Vehicle::typeName(kind) << " identified in " << laneName << " lane\n";
        return kind;
    }

    // Position (1 = front of queue) of the emergency vehicle in its lane.
    int detectEmergencyPosition(const char *laneName, int vehicleCount, const int used[], int usedCount)
    {
        if (mode == 3)
        {
            // In file-input mode, locate the usedCount-th emergency vehicle in the lane, counted from the front
            int lane = laneIndex(laneName);
            int seen = 0;

            if (lane >= 0)
            {
                for (int i = 0; i < fileInputCount[lane]; i++)
                {
                    if (fileInputEmergency[lane][i] != UNKNOWN)
                    {
                        if (seen == usedCount)
                        {
                            return i + 1;
                        }

                        seen++;
                    }
                }
            }

            return 1;
        }

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

// Module 4:: Emergency Routing (priority heap + green corridor)

// MODULE 4 (part 1): EMERGENCY VEHICLE PRIORITY

class EmergencyManager : public Reportable
{
private:
    Vehicle heap[MAX_EMERGENCIES];
    int heapSize;
    int totalInserted;
    int totalExtracted;

    // Higher priority first; then earlier arrival; then smaller ID.
    bool hasHigherPriority(const Vehicle &a, const Vehicle &b) const
    {
        if (a.getPriority() != b.getPriority())
        {
            return a.getPriority() > b.getPriority();
        }

        if (a.getArrivalTime() != b.getArrivalTime())
        {
            return a.getArrivalTime() < b.getArrivalTime();
        }

        return a.getId() < b.getId();
    }

    // Move a new vehicle up until its parent has higher priority.
    void siftUp(int index)
    {
        while (index > 0)
        {
            int parent = (index - 1) / 2;

            if (!hasHigherPriority(heap[index], heap[parent]))
            {
                break;
            }

            swapValues(heap[index], heap[parent]);
            index = parent;
        }
    }

    // Move the root down until both children have lower priority.
    void siftDown(int index)
    {
        while (true)
        {
            int leftChild = 2 * index + 1;
            int rightChild = 2 * index + 2;
            int highest = index;

            if (leftChild < heapSize && hasHigherPriority(heap[leftChild], heap[highest]))
            {
                highest = leftChild;
            }

            if (rightChild < heapSize && hasHigherPriority(heap[rightChild], heap[highest]))
            {
                highest = rightChild;
            }

            if (highest == index)
            {
                break;
            }

            swapValues(heap[index], heap[highest]);
            index = highest;
        }
    }

public:
    EmergencyManager()
    {
        heapSize = 0;
        totalInserted = 0;
        totalExtracted = 0;
    }

    bool isEmpty() const
    {
        return heapSize == 0;
    }

    int getCount() const
    {
        return heapSize;
    }

    bool insertEmergency(const Vehicle &vehicle)
    {
        if (heapSize == MAX_EMERGENCIES)
        {
            cout << "Emergency queue is full.\n";
            return false;
        }

        if (!vehicle.isEmergency())
        {
            cout << "Rejected: not an emergency vehicle.\n";
            return false;
        }

        heap[heapSize] = vehicle;
        siftUp(heapSize);
        heapSize++;
        totalInserted++;

        return true;
    }

    bool extractHighestPriority(Vehicle &vehicle)
    {
        if (isEmpty())
        {
            return false;
        }

        vehicle = heap[0];
        heapSize--;

        if (heapSize > 0)
        {
            heap[0] = heap[heapSize];
            siftDown(0);
        }

        totalExtracted++;
        return true;
    }

    // Raw heap array order (NOT fully sorted).
    void displayPriorityQueue() const
    {
        cout << "\nEmergency priority heap (array order):\n";

        if (heapSize == 0)
        {
            cout << "  No emergency vehicles waiting.\n";
            return;
        }

        for (int i = 0; i < heapSize; i++)
        {
            cout << "  [" << i << "] ID " << heap[i].getId() << " | " << Vehicle::typeName(heap[i].getType()) << " | Priority " << heap[i].getPriority() << '\n';
        }

        cout << "Note: heap-array order is not fully sorted.\n";
    }

    void displayReport() const
    {
        cout << "Emergency vehicles waiting: " << heapSize << '\n';
        cout << "Total inserted: " << totalInserted << '\n';
        cout << "Total dispatched: " << totalExtracted << '\n';
    }

    const char *moduleName() const
    {
        return "Module 4 - Emergency Priority";
    }
};

// PENDING (Phase III): change priority of a queued vehicle, heap tree view,
// non-destructive dispatch order.

// MODULE 4 (part 2): GREEN CORRIDOR

class GreenCorridor : public Reportable
{
private:
    int distanceMap[MAX_INTERSECTIONS][MAX_INTERSECTIONS]; // 0 = no road
    bool blocked[MAX_INTERSECTIONS][MAX_INTERSECTIONS];
    const char *intersectionNames[MAX_INTERSECTIONS];

    bool roadAvailable(int a, int b) const
    {
        return distanceMap[a][b] > 0 && !blocked[a][b];
    }

public:
    GreenCorridor()
    {
        for (int i = 0; i < MAX_INTERSECTIONS; i++)
        {
            for (int j = 0; j < MAX_INTERSECTIONS; j++)
            {
                distanceMap[i][j] = 0;
                blocked[i][j] = false;
            }
        }

        intersectionNames[0] = "City Hospital";
        intersectionNames[1] = "Central Square";
        intersectionNames[2] = "Market Road";
        intersectionNames[3] = "Railway Station";
        intersectionNames[4] = "Fire Station";
        intersectionNames[5] = "Bus Stand";
        intersectionNames[6] = "College Road";
        intersectionNames[7] = "Civil Lines";
        intersectionNames[8] = "Sports Complex";
        intersectionNames[9] = "Stadium Gate";
    }

    bool validIntersection(int id) const
    {
        return id >= 0 && id < MAX_INTERSECTIONS;
    }

    const char *getName(int id) const
    {
        return validIntersection(id) ? intersectionNames[id] : "Invalid";
    }

    bool addRoad(int from, int to, int distance)
    {
        if (!validIntersection(from) || !validIntersection(to) || from == to || distance <= 0)
        {
            return false;
        }

        distanceMap[from][to] = distance;
        distanceMap[to][from] = distance;
        return true;
    }

    bool blockRoad(int from, int to)
    {
        if (!validIntersection(from) || !validIntersection(to) || distanceMap[from][to] == 0)
        {
            return false;
        }

        blocked[from][to] = true;
        blocked[to][from] = true;
        return true;
    }
    
// Unblock a previously blocked road
bool unblockRoad(int from, int to)
{
    if (!validIntersection(from) || !validIntersection(to) ||
        distanceMap[from][to] == 0)
    {
        return false;
    }

    blocked[from][to] = false;
    blocked[to][from] = false;

    return true;
}


    void setupDemoNetwork()
    {
        addRoad(0, 1, 4);
        addRoad(1, 2, 3);
        addRoad(2, 3, 5);
        addRoad(3, 4, 4);
        addRoad(0, 5, 6);
        addRoad(5, 6, 3);
        addRoad(6, 4, 7);
        addRoad(1, 7, 2);
        addRoad(7, 8, 3);
        addRoad(8, 4, 5);
        addRoad(5, 9, 2);
        addRoad(9, 8, 4);
    }

    // BFS: route with the FEWEST roads.
    bool findShortestRoute(int source, int destination, int route[], int &routeLength) const
    {
        routeLength = 0;

        if (!validIntersection(source) || !validIntersection(destination))
        {
            return false;
        }

        int visited[MAX_INTERSECTIONS] = {0};
        int parent[MAX_INTERSECTIONS];
        int queue[MAX_INTERSECTIONS];
        int front = 0;
        int rear = 0;

        for (int i = 0; i < MAX_INTERSECTIONS; i++)
        {
            parent[i] = -1;
        }

        visited[source] = 1;
        queue[rear++] = source;

        while (front < rear)
        {
            int current = queue[front++];

            if (current == destination)
            {
                break;
            }

            for (int next = 0; next < MAX_INTERSECTIONS; next++)
            {
                if (roadAvailable(current, next) && !visited[next])
                {
                    visited[next] = 1;
                    parent[next] = current;
                    queue[rear++] = next;
                }
            }
        }

        if (!visited[destination])
        {
            return false;
        }

        // Walk back from the destination, then reverse.
        int reversePath[MAX_INTERSECTIONS];
        int length = 0;
        int current = destination;

        while (current != -1 && length < MAX_INTERSECTIONS)
        {
            reversePath[length++] = current;
            current = parent[current];
        }

        for (int i = 0; i < length; i++)
        {
            route[i] = reversePath[length - 1 - i];
        }

        routeLength = length;
        return true;
    }

    int pathDistance(const int route[], int length) const
    {
        int total = 0;

        for (int i = 0; i + 1 < length; i++)
        {
            total += distanceMap[route[i]][route[i + 1]];
        }

        return total;
    }

    void displayRoute(int source, int destination) const
    {
        int route[MAX_INTERSECTIONS];
        int routeLength = 0;

        printHeader("GREEN CORRIDOR ROUTE");

        if (!findShortestRoute(source, destination, route, routeLength))
        {
            cout << "No available route between those intersections.\n";
            return;
        }

        cout << "Shortest route (fewest roads):\n  ";

        for (int i = 0; i < routeLength; i++)
        {
            cout << intersectionNames[route[i]] << "(" << route[i] << ")";

            if (i < routeLength - 1)
            {
                cout << " -> ";
            }
        }

        cout << "\n  Roads used: " << routeLength - 1 << " | Distance: " << pathDistance(route, routeLength) << " units\n";
    }

    void displayNetwork() const
    {
        cout << "\nRoad network (neighbour:distance, X = blocked):\n";

        for (int i = 0; i < MAX_INTERSECTIONS; i++)
        {
            cout << "  " << i << " " << setw(16) << left << intersectionNames[i] << right << ": ";

            for (int j = 0; j < MAX_INTERSECTIONS; j++)
            {
                if (distanceMap[i][j] > 0)
                {
                    cout << j << ":" << distanceMap[i][j] << (blocked[i][j] ? "X" : "") << "  ";
                }
            }

            cout << '\n';
        }
    }

    void displayReport() const
    {
        int roads = 0;
        int blockedRoads = 0;

        for (int i = 0; i < MAX_INTERSECTIONS; i++)
        {
            for (int j = i + 1; j < MAX_INTERSECTIONS; j++)
            {
                if (distanceMap[i][j] > 0)
                {
                    roads++;

                    if (blocked[i][j])
                    {
                        blockedRoads++;
                    }
                }
            }
        }

        cout << "Intersections: " << MAX_INTERSECTIONS << '\n';
        cout << "Roads: " << roads << " (blocked: " << blockedRoads << ")\n";
    }

    const char *moduleName() const
    {
        return "Module 4 - Green Corridor";
    }
};

// PENDING (Phase III): Dijkstra shortest-distance route, DFS reachability
// check, green-hold marking of corridor intersections, unblock road.

// Module 5:: Analytics & Reports

struct ServiceRecord
{
    int vehicleId;
    int laneCode; // 0 = emergency corridor, 1-4 = N, S, E, W
    int serviceTime;
    int waitingTime;
    bool emergency;
    VehicleType type;
};

const char *laneCodeName(int code)
{
    if (code == 0)
    {
        return "Corridor";
    }

    if (code == 1)
    {
        return "North";
    }

    if (code == 2)
    {
        return "South";
    }

    if (code == 3)
    {
        return "East";
    }

    if (code == 4)
    {
        return "West";
    }

    return "Unknown";
}

class PerformanceAnalytics : public Reportable
{
private:
    ServiceRecord *records;
    int recordCount;
    int capacity;

    int normalVehiclesServed;
    int emergencyVehiclesServed;

    int totalWaitingTime;
    int maximumWaitingTime;
    int minimumWaitingTime;

    // Doubles the array size when it is full.
    bool resize(int newCapacity)
    {
        if (newCapacity <= capacity)
        {
            return false;
        }

        ServiceRecord *newRecords = new (nothrow) ServiceRecord[newCapacity];

        if (newRecords == NULL)
        {
            return false;
        }

        for (int i = 0; i < recordCount; i++)
        {
            newRecords[i] = records[i];
        }

        delete[] records;
        records = newRecords;
        capacity = newCapacity;

        return true;
    }

public:
    PerformanceAnalytics()
    {
        capacity = 10;
        recordCount = 0;
        normalVehiclesServed = 0;
        emergencyVehiclesServed = 0;
        totalWaitingTime = 0;
        maximumWaitingTime = 0;
        minimumWaitingTime = 0;

        records = new ServiceRecord[capacity];
    }

    ~PerformanceAnalytics()
    {
        delete[] records;
    }

    // Owns raw memory, so copying is forbidden (Rule of Three).
    PerformanceAnalytics(const PerformanceAnalytics &) = delete;
    PerformanceAnalytics &operator=(const PerformanceAnalytics &) = delete;

    int getRecordCount() const
    {
        return recordCount;
    }

    int getNormalServed() const
    {
        return normalVehiclesServed;
    }

    int getEmergencyServed() const
    {
        return emergencyVehiclesServed;
    }

    int getMaxWait() const
    {
        return maximumWaitingTime;
    }

    int getMinWait() const
    {
        return minimumWaitingTime;
    }

    const ServiceRecord &getRecord(int index) const
    {
        return records[index];
    }

    bool addRecord(int id, int lane, int serviceTime, int waitTime, bool emergency, VehicleType type = UNKNOWN)
    {
        if (recordCount == capacity)
        {
            if (!resize(capacity * 2))
            {
                cout << "Unable to expand history storage.\n";
                return false;
            }
        }

        records[recordCount].vehicleId = id;
        records[recordCount].laneCode = lane;
        records[recordCount].serviceTime = serviceTime;
        records[recordCount].waitingTime = waitTime;
        records[recordCount].emergency = emergency;
        records[recordCount].type = type;
        recordCount++;

        if (emergency)
        {
            emergencyVehiclesServed++;
        }
        else
        {
            normalVehiclesServed++;
        }

        totalWaitingTime += waitTime;

        if (recordCount == 1)
        {
            minimumWaitingTime = waitTime;
            maximumWaitingTime = waitTime;
        }
        else
        {
            if (waitTime < minimumWaitingTime)
            {
                minimumWaitingTime = waitTime;
            }

            if (waitTime > maximumWaitingTime)
            {
                maximumWaitingTime = waitTime;
            }
        }

        return true;
    }

    int linearSearchVehicle(int id) const
    {
        for (int i = 0; i < recordCount; i++)
        {
            if (records[i].vehicleId == id)
            {
                return i;
            }
        }

        return -1;
    }

    double averageWaitingTime() const
    {
        if (recordCount == 0)
        {
            return 0.0;
        }

        return static_cast<double>(totalWaitingTime) / recordCount;
    }

    void searchAndDisplayVehicle(int id) const
    {
        int index = linearSearchVehicle(id);

        if (index == -1)
        {
            cout << "Vehicle " << id << " not found in service history.\n";
            return;
        }

        cout << "\nVehicle history found:\n";
        cout << "Vehicle ID: " << records[index].vehicleId << '\n';
        cout << "Lane: " << laneCodeName(records[index].laneCode) << '\n';
        cout << "Service time: " << records[index].serviceTime << "s\n";
        cout << "Waiting time: " << records[index].waitingTime << "s\n";
        cout << "Type: " << Vehicle::typeName(records[index].type) << '\n';
        cout << "Emergency: " << (records[index].emergency ? "Yes" : "No") << '\n';
    }

    // Bubble sort on a COPY, so the real history keeps its order.
    void displaySortedHistory() const
    {
        printHeader("HISTORY SORTED BY WAIT TIME (bubble sort)");

        if (recordCount == 0)
        {
            cout << "No history to sort.\n";
            return;
        }

        ServiceRecord *sorted = new (nothrow) ServiceRecord[recordCount];

        if (sorted == NULL)
        {
            cout << "Not enough memory to sort.\n";
            return;
        }

        for (int i = 0; i < recordCount; i++)
        {
            sorted[i] = records[i];
        }

        for (int i = 0; i < recordCount - 1; i++)
        {
            bool swapped = false;

            for (int j = 0; j < recordCount - i - 1; j++)
            {
                if (sorted[j].waitingTime > sorted[j + 1].waitingTime)
                {
                    swapValues(sorted[j], sorted[j + 1]);
                    swapped = true;
                }
            }

            if (!swapped)
            {
                break;
            }
        }

        for (int i = 0; i < recordCount; i++)
        {
            cout << "Vehicle " << setw(5) << sorted[i].vehicleId << " | " << left << setw(9) << laneCodeName(sorted[i].laneCode) << right << " | Wait: " << sorted[i].waitingTime << "s\n";
        }

        delete[] sorted;
    }

    // maxRows = -1 shows every record.
    void displayServiceHistory(int maxRows = -1) const
    {
        printHeader("SERVICE HISTORY");

        if (recordCount == 0)
        {
            cout << "No vehicles have been served.\n";
            return;
        }

        cout << left << setw(12) << "Vehicle ID" << setw(12) << "Lane" << setw(19) << "Type" << setw(14) << "Service Time" << setw(12) << "Wait Time" << setw(10) << "Emergency" << right << '\n';

        int rows = recordCount;

        if (maxRows >= 0 && maxRows < recordCount)
        {
            rows = maxRows;
        }

        for (int i = 0; i < rows; i++)
        {
            cout << left << setw(12) << records[i].vehicleId << setw(12) << laneCodeName(records[i].laneCode) << setw(19) << Vehicle::typeName(records[i].type) << setw(14) << records[i].serviceTime << setw(12) << records[i].waitingTime << setw(10) << (records[i].emergency ? "Yes" : "No") << right << '\n';
        }

        if (rows < recordCount)
        {
            cout << "... " << recordCount - rows << " more records hidden.\n";
        }
    }

    void displayReport() const
    {
        cout << "Normal vehicles served: " << normalVehiclesServed << '\n';
        cout << "Emergency vehicles served: " << emergencyVehiclesServed << '\n';
        cout << "Total vehicles served: " << recordCount << '\n';
        cout << "Average waiting time: " << fixed << setprecision(2) << averageWaitingTime() << " seconds\n";

        if (recordCount > 0)
        {
            cout << "Minimum wait: " << minimumWaitingTime << " seconds\n";
            cout << "Maximum wait: " << maximumWaitingTime << " seconds\n";
        }
    }

    const char *moduleName() const
    {
        return "Module 5 - Analytics";
    }
};

// Module 6:: Fixed-Timer vs Adaptive Comparison

struct StrategyResult
{
    char name[20];
    int served;
    int totalWait;
    int maxWait;
    int emergencyServed;
    int emergencyWait;
    int kindServed[3]; // 0 = ambulance, 1 = fire truck, 2 = police car
    int kindWait[3];
    int phasesUsed;
    int greenGiven;
    int greenWasted;
};

// Replays the SAME detected traffic under two strategies and compares them.
// Every detected vehicle is waiting at time 0, so wait time = departure time.
class StrategyComparison : public Reportable
{
private:
    int total[4]; // 0..3 = North, South, East, West
    int emergencyCount[4];
    VehicleType emergencyKind[4][LANE_CAPACITY + 1]; // [lane][position 1..count], UNKNOWN = ordinary
    int count[4];                                    // ordinary vehicles still waiting in the normal lane
    int emWaiting[4];                                // emergency vehicles still waiting in the green corridor lane
    int emScan[4];                                   // next position to look at in emergencyKind[lane][]
    int nextPos[4];                                  // next position in the normal queue (fixed road without a lane)
    bool flagsActive;                                // emergency vehicles wait inside the normal queue
    bool fixedHasLane;                               // does the fixed road ALSO have a green corridor lane?
    int fixedGreen;
    bool ready;
    StrategyResult fixedResult;
    StrategyResult adaptiveResult;

    void resetResult(StrategyResult &r, const char *name)
    {
        strncpy(r.name, name, sizeof(r.name) - 1);
        r.name[sizeof(r.name) - 1] = '\0';
        r.served = 0;
        r.totalWait = 0;
        r.maxWait = 0;
        r.emergencyServed = 0;
        r.emergencyWait = 0;

        for (int k = 0; k < 3; k++)
        {
            r.kindServed[k] = 0;
            r.kindWait[k] = 0;
        }

        r.phasesUsed = 0;
        r.greenGiven = 0;
        r.greenWasted = 0;
    }

    // mode 0 : adaptive road - the priority heap dispatches the emergency vehicles through the
    //          green corridor lane, only ordinary vehicles queue
    // mode 1 : fixed road WITH a green corridor lane - emergency vehicles wait in it for their
    //          direction's green (no signal priority)
    // mode 2 : fixed road, normal road only - an emergency vehicle waits inside the normal queue
    void loadLanes(int mode)
    {
        flagsActive = (mode == 2);

        for (int lane = 0; lane < 4; lane++)
        {
            nextPos[lane] = 1;
            emScan[lane] = 1;
            count[lane] = (mode == 2) ? total[lane] : total[lane] - emergencyCount[lane];
            emWaiting[lane] = (mode == 1) ? emergencyCount[lane] : 0;
        }
    }

    int remaining() const
    {
        return count[0] + count[1] + count[2] + count[3] +
               emWaiting[0] + emWaiting[1] + emWaiting[2] + emWaiting[3];
    }

    bool directionEmpty(TrafficPhase phase) const
    {
        if (phase == NORTH_SOUTH_PHASE)
        {
            return count[0] == 0 && count[1] == 0;
        }

        return count[2] == 0 && count[3] == 0;
    }

    // kind = UNKNOWN for an ordinary vehicle, otherwise the emergency vehicle type
    void record(StrategyResult &r, int wait, VehicleType kind)
    {
        r.served++;
        r.totalWait += wait;

        if (wait > r.maxWait)
        {
            r.maxWait = wait;
        }

        if (Vehicle::isEmergencyType(kind))
        {
            int slot = kind - AMBULANCE;
            r.emergencyServed++;
            r.emergencyWait += wait;
            r.kindServed[slot]++;
            r.kindWait[slot] += wait;
        }
    }

    // Same rule as EmergencyManager: higher priority first, then smaller vehicle ID.
    // Vehicle IDs run 1..N across the lanes (North, South, East, West).
    bool dispatchesBefore(int laneA, int posA, int laneB, int posB, const int startId[4]) const
    {
        int priorityA = Vehicle::defaultPriority(emergencyKind[laneA][posA]);
        int priorityB = Vehicle::defaultPriority(emergencyKind[laneB][posB]);

        if (priorityA != priorityB)
        {
            return priorityA > priorityB;
        }

        return startId[laneA] + posA < startId[laneB] + posB;
    }

    // Normal lane: releases one vehicle every SECONDS_PER_VEHICLE seconds.
    int serveLane(int lane, int slots, int clock, StrategyResult &r)
    {
        int served = 0;

        while (served < slots && count[lane] > 0)
        {
            served++;

            VehicleType kind = UNKNOWN;

            if (flagsActive)
            {
                kind = emergencyKind[lane][nextPos[lane]];
            }

            record(r, clock + served * SECONDS_PER_VEHICLE, kind);
            nextPos[lane]++;
            count[lane]--;
        }

        return served;
    }

    // Green corridor lane (fixed timer): its own queue, released during the lane's direction
    // green at the same pace, side by side with the normal lane. No signal priority.
    int serveEmergencyLane(int lane, int slots, int clock, StrategyResult &r)
    {
        int served = 0;

        while (served < slots && emWaiting[lane] > 0)
        {
            while (emergencyKind[lane][emScan[lane]] == UNKNOWN)
            {
                emScan[lane]++;
            }

            served++;
            record(r, clock + served * SECONDS_PER_VEHICLE, emergencyKind[lane][emScan[lane]]);
            emScan[lane]++;
            emWaiting[lane]--;
        }

        return served;
    }

    // One green phase: both lanes of a direction move side by side.
    void runPhase(TrafficPhase phase, int green, int &clock, StrategyResult &r, bool skipIfEmpty)
    {
        if (skipIfEmpty && directionEmpty(phase))
        {
            return;
        }

        int laneA = (phase == NORTH_SOUTH_PHASE) ? 0 : 2;
        int laneB = laneA + 1;
        int slots = green / SECONDS_PER_VEHICLE;

        int servedA = serveLane(laneA, slots, clock, r);
        int servedB = serveLane(laneB, slots, clock, r);
        int emergencyA = serveEmergencyLane(laneA, slots, clock, r);
        int emergencyB = serveEmergencyLane(laneB, slots, clock, r);

        int busiest = servedA;

        if (servedB > busiest)
        {
            busiest = servedB;
        }

        if (emergencyA > busiest)
        {
            busiest = emergencyA;
        }

        if (emergencyB > busiest)
        {
            busiest = emergencyB;
        }

        r.phasesUsed++;
        r.greenGiven += green;
        r.greenWasted += green - busiest * SECONDS_PER_VEHICLE;

        clock += green + PhaseController::transitionTime();
    }

    // Strategy 1: same green time for both directions, fixed order, and an empty direction
    // still gets its green. Emergency vehicles get NO signal priority. On a normal road they wait
    // inside the normal queue; with fixedHasLane they wait in the green corridor lane instead and
    // cross when their direction turns green.
    void runFixed()
    {
        resetResult(fixedResult, "Fixed-timer");
        loadLanes(fixedHasLane ? 1 : 2);

        int clock = 0;
        int guard = 0;

        while (remaining() > 0 && guard < 1000)
        {
            runPhase(NORTH_SOUTH_PHASE, fixedGreen, clock, fixedResult, false);

            if (remaining() == 0)
            {
                break;
            }

            runPhase(EAST_WEST_PHASE, fixedGreen, clock, fixedResult, false);
            guard++;
        }
    }

    // Strategy 2: exactly what Module 7 does - emergency corridor first,
    // then green time from the longest lane, busier direction first,
    // empty directions skipped.
    void runAdaptive(const GreenCorridor &corridor)
    {
        resetResult(adaptiveResult, "Adaptive");
        loadLanes(0);

        const int entryByLane[4] = {1, 3, 8, 5};
        int clock = 0;

        // Emergency vehicles leave in heap order: highest priority first
        // (ambulance, fire truck, police car), then smaller vehicle ID.
        int startId[4];
        int offset = 0;

        for (int lane = 0; lane < 4; lane++)
        {
            startId[lane] = offset;
            offset += total[lane];
        }

        int order[MAX_EMERGENCIES][2]; // [i][0] = lane, [i][1] = position
        int orderCount = 0;

        for (int lane = 0; lane < 4; lane++)
        {
            for (int p = 1; p <= total[lane]; p++)
            {
                if (emergencyKind[lane][p] != UNKNOWN && orderCount < MAX_EMERGENCIES)
                {
                    order[orderCount][0] = lane;
                    order[orderCount][1] = p;
                    orderCount++;
                }
            }
        }

        // selection sort with the same rule the heap uses
        for (int i = 0; i < orderCount - 1; i++)
        {
            int best = i;

            for (int j = i + 1; j < orderCount; j++)
            {
                if (dispatchesBefore(order[j][0], order[j][1], order[best][0], order[best][1], startId))
                {
                    best = j;
                }
            }

            swapValues(order[i][0], order[best][0]);
            swapValues(order[i][1], order[best][1]);
        }

        for (int i = 0; i < orderCount; i++)
        {
            int lane = order[i][0];
            VehicleType kind = emergencyKind[lane][order[i][1]];
            int route[MAX_INTERSECTIONS];
            int routeLength = 0;

            if (corridor.findShortestRoute(entryByLane[lane], emergencyDestination(kind), route, routeLength))
            {
                record(adaptiveResult, clock + SECONDS_PER_VEHICLE, kind); // crosses the stop line
                clock += EMERGENCY_CROSSING_TIME;                          // junction held until it has cleared
            }
        }

        TrafficAnalyzer analyzer;
        analyzer.setGreenLimits(10, 60);

        int guard = 0;

        while (remaining() > 0 && guard < 1000)
        {
            int nsLongest = (count[0] > count[1]) ? count[0] : count[1];
            int ewLongest = (count[2] > count[3]) ? count[2] : count[3];
            int nsGreen = analyzer.calculateGreenTime(nsLongest);
            int ewGreen = analyzer.calculateGreenTime(ewLongest);

            TrafficPhase first = analyzer.choosePhase(count[0] + count[1], count[2] + count[3]);
            TrafficPhase second = (first == NORTH_SOUTH_PHASE) ? EAST_WEST_PHASE : NORTH_SOUTH_PHASE;

            runPhase(first, (first == NORTH_SOUTH_PHASE) ? nsGreen : ewGreen, clock, adaptiveResult, true);
            runPhase(second, (second == NORTH_SOUTH_PHASE) ? nsGreen : ewGreen, clock, adaptiveResult, true);
            guard++;
        }
    }

    static double average(int sum, int n)
    {
        return n > 0 ? static_cast<double>(sum) / n : 0.0;
    }

    void printRow(const char *label, double a, double b, int decimals) const
    {
        cout << left << setw(32) << label << fixed << setprecision(decimals)
             << setw(16) << a << setw(16) << b << right << '\n';
    }

    void printTextRow(const char *label, const char *a, const char *b) const
    {
        cout << left << setw(32) << label << setw(16) << a << setw(16) << b << right << '\n';
    }

    void printChange(const char *label, double fixedValue, double adaptiveValue) const
    {
        if (fixedValue <= 0)
        {
            return;
        }

        double percent = (fixedValue - adaptiveValue) * 100.0 / fixedValue;

        cout << "  " << label << ": ";

        if (percent >= 0)
        {
            cout << "reduced by " << fixed << setprecision(1) << percent << "%\n";
        }
        else
        {
            cout << "increased by " << fixed << setprecision(1) << -percent << "%\n";
        }
    }

public:
    StrategyComparison()
    {
        for (int lane = 0; lane < 4; lane++)
        {
            total[lane] = 0;
            emergencyCount[lane] = 0;
            count[lane] = 0;
            emWaiting[lane] = 0;
            emScan[lane] = 1;
            nextPos[lane] = 1;

            for (int p = 0; p <= LANE_CAPACITY; p++)
            {
                emergencyKind[lane][p] = UNKNOWN;
            }
        }

        flagsActive = false;
        fixedHasLane = false;
        fixedGreen = 30;
        ready = false;
        resetResult(fixedResult, "Fixed-timer");
        resetResult(adaptiveResult, "Adaptive");
    }

    // Takes the traffic exactly as the sensors detected it.
    void setTraffic(const int laneCounts[4], const int emergencies[4],
                    const int positions[4][VehicleDetector::MAX_EMERGENCY_PER_LANE],
                    const VehicleType kinds[4][VehicleDetector::MAX_EMERGENCY_PER_LANE])
    {
        for (int lane = 0; lane < 4; lane++)
        {
            total[lane] = laneCounts[lane];
            emergencyCount[lane] = 0;

            for (int p = 0; p <= LANE_CAPACITY; p++)
            {
                emergencyKind[lane][p] = UNKNOWN;
            }

            for (int k = 0; k < emergencies[lane]; k++)
            {
                int pos = positions[lane][k];

                if (pos >= 1 && pos <= total[lane] && emergencyKind[lane][pos] == UNKNOWN &&
                    Vehicle::isEmergencyType(kinds[lane][k]))
                {
                    emergencyKind[lane][pos] = kinds[lane][k];
                    emergencyCount[lane]++;
                }
            }
        }

        ready = true;
    }

    // Same as compare() but with no prompt and no printing (front end / batch mode).
    void evaluate(const GreenCorridor &corridor, int fixedGreenSeconds, bool fixedLane = false)
    {
        if (!ready)
        {
            return;
        }

        if (fixedGreenSeconds < 10)
        {
            fixedGreenSeconds = 10;
        }

        if (fixedGreenSeconds > 60)
        {
            fixedGreenSeconds = 60;
        }

        fixedGreen = fixedGreenSeconds;
        fixedHasLane = fixedLane;
        runFixed();
        runAdaptive(corridor);
    }

    int getFixedGreen() const
    {
        return fixedGreen;
    }

    bool getFixedHasLane() const
    {
        return fixedHasLane;
    }

    const StrategyResult &getFixedResult() const
    {
        return fixedResult;
    }

    const StrategyResult &getAdaptiveResult() const
    {
        return adaptiveResult;
    }

    void compare(const GreenCorridor &corridor)
    {
        printHeader("FIXED-TIMER vs ADAPTIVE COMPARISON");

        if (!ready)
        {
            cout << "No traffic data to compare.\n";
            return;
        }

        fixedGreen = readInt("Fixed-timer green time per direction (10-60 seconds): ", 10, 60);

        runFixed();
        runAdaptive(corridor);
        displayReport();
    }

    void displayReport() const
    {
        if (!ready)
        {
            cout << "No traffic data to compare.\n";
            return;
        }

        cout << "\nSame traffic for both: North " << total[0] << ", South " << total[1] << ", East " << total[2] << ", West " << total[3] << "  (emergency vehicles: " << emergencyCount[0] + emergencyCount[1] + emergencyCount[2] + emergencyCount[3] << ")\n\n";

        cout << left << setw(32) << "Metric" << setw(16) << fixedResult.name << setw(16) << adaptiveResult.name << right << '\n';
        cout << "--------------------------------------------------------------\n";

        printRow("Vehicles served", fixedResult.served, adaptiveResult.served, 0);
        printRow("Average wait (s)", average(fixedResult.totalWait, fixedResult.served), average(adaptiveResult.totalWait, adaptiveResult.served), 2);
        printRow("Longest wait = clear time (s)", fixedResult.maxWait, adaptiveResult.maxWait, 0);

        if (fixedResult.emergencyServed > 0 && adaptiveResult.emergencyServed > 0)
        {
            printRow("Avg emergency wait (s)", average(fixedResult.emergencyWait, fixedResult.emergencyServed), average(adaptiveResult.emergencyWait, adaptiveResult.emergencyServed), 2);
        }
        else
        {
            printTextRow("Avg emergency wait (s)", "-", "-");
        }

        printRow("Green phases used", fixedResult.phasesUsed, adaptiveResult.phasesUsed, 0);
        printRow("Green seconds given", fixedResult.greenGiven, adaptiveResult.greenGiven, 0);
        printRow("Green seconds wasted", fixedResult.greenWasted, adaptiveResult.greenWasted, 0);

        cout << "\nAdaptive compared with Fixed-timer:\n";
        printChange("Average wait", average(fixedResult.totalWait, fixedResult.served), average(adaptiveResult.totalWait, adaptiveResult.served));
        printChange("Time to clear all vehicles", fixedResult.maxWait, adaptiveResult.maxWait);
        printChange("Green seconds wasted", fixedResult.greenWasted, adaptiveResult.greenWasted);

        if (fixedResult.emergencyServed > 0 && adaptiveResult.emergencyServed > 0)
        {
            printChange("Emergency wait", average(fixedResult.emergencyWait, fixedResult.emergencyServed), average(adaptiveResult.emergencyWait, adaptiveResult.emergencyServed));
        }

        cout << "\nModel: all detected vehicles are waiting at time 0 (no new arrivals).\n";
        cout << "Fixed-timer: same green for both directions, an empty direction still receives\n";
        cout << "its green. Emergency vehicles get no signal priority and wait in the normal queue.\n";
    }

    const char *moduleName() const
    {
        return "Module 6 - Strategy Comparison";
    }
};

// Module 7:: Simulation Engine

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
    StrategyComparison comparison;

    int simulationClock;
    int totalInitialVehicles;
    int emergencyRouteFailures;

    // Index 0..3 = North, South, East, West
    int laneCounts[4];
    int laneStartId[4];
    int emergencyCounts[4];
    int usedPositions[4][VehicleDetector::MAX_EMERGENCY_PER_LANE];
    VehicleType emergencyKinds[4][VehicleDetector::MAX_EMERGENCY_PER_LANE];

    // Records kept for the front end report (batch mode)
    struct PhaseLog
    {
        int cycle;
        int phase;
        int green;
        int start;
        int laneA;
        int laneB;
        int servedA;
        int servedB;
    };

    struct EmergencyLog
    {
        int start; // signals turn red for this vehicle
        int end;   // junction released
        int eta;   // arrives at its destination (crossing time + route length)
        int id;
        int type;
        int priority;
        int destination;
        int lane;
        int distance;
        int serviceTime;
        int routeLength;
        int route[MAX_INTERSECTIONS];
    };

    PhaseLog phaseLog[100];
    int phaseLogCount;
    EmergencyLog emergencyLog[MAX_EMERGENCIES];
    int emergencyLogCount;
    int currentCycle;

    Lane &laneByCode(int code)
    {
        if (code == NORTH_LANE)
            return north;
        if (code == SOUTH_LANE)
            return south;
        if (code == EAST_LANE)
            return east;
        return west;
    }

    int northSouthCount() const
    {
        return north.getCount() + south.getCount();
    }

    int eastWestCount() const
    {
        return east.getCount() + west.getCount();
    }

    int normalRemaining() const
    {
        return northSouthCount() + eastWestCount();
    }

    // Which lane (0..3) did this vehicle ID come from?
    int laneOfVehicle(int id) const
    {
        for (int lane = 0; lane < 4; lane++)
        {
            if (id > laneStartId[lane] &&
                id <= laneStartId[lane] + laneCounts[lane])
            {
                return lane;
            }
        }

        return 0;
    }

    // ---------- STEP 1: DETECTION ----------

    void detectTraffic()
    {
        const char *names[4] = {"North", "South", "East", "West"};

        // Sensor 1: vehicle count per lane.
        int offset = 0;

        for (int lane = 0; lane < 4; lane++)
        {
            laneCounts[lane] = detector.detectVehicles(names[lane]);
            laneStartId[lane] = offset;
            offset += laneCounts[lane];
        }

        totalInitialVehicles = offset;

        // Create the vehicle objects. IDs run 1..N across all lanes.
        for (int lane = 0; lane < 4; lane++)
        {
            for (int i = 0; i < laneCounts[lane]; i++)
            {
                VehicleType kind = detector.vehicleTypeAt(names[lane], i);
                laneByCode(lane + 1).addVehicle(
                    Vehicle(laneStartId[lane] + i + 1, kind, 0));
            }
        }

        // Sensor 2: how many sirens in each lane.
        for (int lane = 0; lane < 4; lane++)
        {
            emergencyCounts[lane] =
                detector.detectEmergencies(names[lane], laneCounts[lane]);
        }

        // For every siren: which vehicle is it? Move it out of the
        // normal queue and into the emergency priority heap.
        for (int lane = 0; lane < 4; lane++)
        {
            for (int k = 0; k < emergencyCounts[lane]; k++)
            {
                int position = detector.detectEmergencyPosition(
                    names[lane], laneCounts[lane], usedPositions[lane], k);
                usedPositions[lane][k] = position;

                // Which kind is it: ambulance, fire truck or police car?
                VehicleType emergencyKind = detector.detectEmergencyKind(names[lane], position);
                emergencyKinds[lane][k] = emergencyKind;

                int globalId = laneStartId[lane] + position;
                Vehicle removed;
                bool found = laneByCode(lane + 1).extractVehicleById(
                    globalId, removed);

                if (found)
                {
                    emergencyManager.insertEmergency(
                        Vehicle(globalId, emergencyKind, 0));
                }
            }
        }
    }

    void displayInitialQueues() const
    {
        cout << "\n--- INITIAL TRAFFIC AFTER EMERGENCY TRANSFER ---\n";
        cout << "North normal queue: " << north.getCount() << '\n';
        cout << "South normal queue: " << south.getCount() << '\n';
        cout << "East normal queue:  " << east.getCount() << '\n';
        cout << "West normal queue:  " << west.getCount() << '\n';
    }

    // ---------- STEP 2: EMERGENCY GREEN CORRIDOR ----------

    void processEmergencyVehicles()
    {
        if (emergencyManager.isEmpty())
        {
            return;
        }

        // Each lane has an entry point. In Phase II, all emergency
        // vehicles use one common destination: City Hospital (node 0).
        const int entryByLane[4] = {1, 3, 8, 5};
        const char *laneNames[4] = {"North", "South", "East", "West"};

        cout << "\n--- GREEN CORRIDOR ACTIVE ---\n";

        signalController.allRed();
        cout << "North-South signal: RED\n";
        cout << "East-West signal: RED\n";
        cout << "Emergency vehicles waiting: "
             << emergencyManager.getCount() << '\n';

        Vehicle emergency;

        while (emergencyManager.extractHighestPriority(emergency))
        {
            int lane = laneOfVehicle(emergency.getId());
            int start = entryByLane[lane];
            int destination = emergencyDestination(emergency.getType()); // depends on the vehicle type

            int route[MAX_INTERSECTIONS];
            int routeLength = 0;

            // operator<< of Vehicle prints type, ID, arrival time and priority.
            cout << "\n"
                 << emergency << " from "
                 << laneNames[lane] << " lane\n";

            if (!greenCorridor.findShortestRoute(start, destination,
                                                 route, routeLength))
            {
                emergencyRouteFailures++;
                cout << "No open route. Vehicle needs an alternative plan.\n";
                continue;
            }

            cout << "Route: ";

            for (int i = 0; i < routeLength; i++)
            {
                cout << greenCorridor.getName(route[i]);

                if (i < routeLength - 1)
                {
                    cout << " -> ";
                }
            }

            int distance = greenCorridor.pathDistance(route, routeLength);
            cout << " (" << distance << " units)\n";

            // The vehicle waits in the green corridor lane, so it crosses the stop line
            // SECONDS_PER_VEHICLE after the signals turn red. The junction is released after
            // EMERGENCY_CROSSING_TIME; the distance of the route does not block this junction.
            int startTime = simulationClock;
            int serviceTime = startTime + SECONDS_PER_VEHICLE;
            int waitTime = serviceTime - emergency.getArrivalTime();

            if (emergencyLogCount < MAX_EMERGENCIES)
            {
                EmergencyLog &log = emergencyLog[emergencyLogCount++];
                log.id = emergency.getId();
                log.type = emergency.getType();
                log.priority = emergency.getPriority();
                log.destination = destination;
                log.lane = lane;
                log.distance = distance;
                log.serviceTime = serviceTime;
                log.start = startTime;
                log.end = startTime + EMERGENCY_CROSSING_TIME;
                log.eta = serviceTime + distance;
                log.routeLength = routeLength;

                for (int i = 0; i < routeLength; i++)
                {
                    log.route[i] = route[i];
                }
            }

            analytics.addRecord(emergency.getId(), 0, serviceTime,
                                waitTime, true, emergency.getType());
            simulationClock = startTime + EMERGENCY_CROSSING_TIME;

            cout << "Emergency vehicle " << emergency.getId()
                 << " crossed the junction from its green corridor lane (t = " << serviceTime
                 << " s, reaches its destination about t = " << serviceTime + distance << " s).\n";
        }

        cout << "Green Corridor is now CLEAR.\n";
        cout << "Normal traffic may resume.\n";
    }

    // ---------- STEP 3: NORMAL TRAFFIC ----------

    // Lanes of one direction move side by side: every lane can pass
    // one vehicle every SECONDS_PER_VEHICLE seconds.
    int serveLane(Lane &lane, int laneCode, int slots)
    {
        int served = 0;
        Vehicle vehicle;

        while (served < slots && lane.serveVehicle(vehicle))
        {
            served++;

            int serviceTime = simulationClock + served * analyzer.getSecondsPerVehicle();
            int waitTime = serviceTime - vehicle.getArrivalTime();

            analytics.addRecord(vehicle.getId(), laneCode, serviceTime,
                                waitTime, false, vehicle.getType());
        }

        return served;
    }

    void runPhase(TrafficPhase phase, int greenTime)
    {
        const char *phaseName = (phase == NORTH_SOUTH_PHASE)
                                    ? "NORTH-SOUTH"
                                    : "EAST-WEST";
        Lane &first = (phase == NORTH_SOUTH_PHASE) ? north : east;
        Lane &second = (phase == NORTH_SOUTH_PHASE) ? south : west;
        int firstCode = (phase == NORTH_SOUTH_PHASE) ? NORTH_LANE : EAST_LANE;
        int secondCode = (phase == NORTH_SOUTH_PHASE) ? SOUTH_LANE : WEST_LANE;

        cout << '\n';

        if (first.isEmpty() && second.isEmpty())
        {
            cout << phaseName << " has no waiting vehicles. Phase skipped.\n";
            return;
        }

        signalController.activatePhase(phase);

        cout << phaseName << " GREEN for " << greenTime << " seconds\n";
        cout << "Other signal: RED\n";

        int slots = greenTime / analyzer.getSecondsPerVehicle();
        int servedFirst = serveLane(first, firstCode, slots);
        int servedSecond = serveLane(second, secondCode, slots);

        if (phaseLogCount < 100)
        {
            PhaseLog &log = phaseLog[phaseLogCount++];
            log.cycle = currentCycle;
            log.phase = phase;
            log.green = greenTime;
            log.start = simulationClock;
            log.laneA = firstCode;
            log.laneB = secondCode;
            log.servedA = servedFirst;
            log.servedB = servedSecond;
        }

        cout << first.getName() << ": " << servedFirst
             << " vehicles served\n";
        cout << second.getName() << ": " << servedSecond
             << " vehicles served\n";

        cout << phaseName << " YELLOW\n";
        signalController.finishPhase();
        cout << phaseName << " RED\n";

        simulationClock += greenTime + PhaseController::transitionTime();
    }

    void runSignalCycle()
    {
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
                                  ? EAST_WEST_PHASE
                                  : NORTH_SOUTH_PHASE;

        cout << "\nSelected phase: "
             << (first == NORTH_SOUTH_PHASE ? "NORTH-SOUTH" : "EAST-WEST")
             << '\n';
        cout << "North-South green time: " << nsGreen << " seconds\n";
        cout << "East-West green time: " << ewGreen << " seconds\n";

        runPhase(first, first == NORTH_SOUTH_PHASE ? nsGreen : ewGreen);
        runPhase(second, second == NORTH_SOUTH_PHASE ? nsGreen : ewGreen);
    }

    // ---------- REPORTS ----------

    void displayFinalQueues() const
    {
        cout << "\n--- FINAL QUEUE REPORT ---\n";
        cout << "North: " << north.getCount() << '\n';
        cout << "South: " << south.getCount() << '\n';
        cout << "East:  " << east.getCount() << '\n';
        cout << "West:  " << west.getCount() << '\n';
    }

    void displaySummary() const
    {
        int totalServed = analytics.getNormalServed() + analytics.getEmergencyServed();

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
    void displayAllReports() const
    {
        const Reportable *reports[] = {
            &north, &south, &east, &west,
            &signalController, &analyzer, &emergencyManager,
            &greenCorridor, &analytics};

        printHeader("ALL MODULE REPORTS");

        const char *lastModule = "";

        for (int i = 0; i < 9; i++)
        {
            if (strcmp(lastModule, reports[i]->moduleName()) != 0)
            {
                cout << "\n-- " << reports[i]->moduleName() << " --\n";
                lastModule = reports[i]->moduleName();
            }

            reports[i]->displayReport();
        }
    }

    void detailedAnalytics()
    {
        analytics.displayServiceHistory(15);
        analytics.displayReport();

        if (analytics.getRecordCount() > 0 &&
            readYesNo("\nSort history by wait time?"))
        {
            analytics.displaySortedHistory();
        }

        if (readYesNo("\nShow all module reports?"))
        {
            displayAllReports();
        }
    }

public:
    TrafficSimulation()
        : north("North"),
          south("South"),
          east("East"),
          west("West")
    {
        simulationClock = 0;
        totalInitialVehicles = 0;
        emergencyRouteFailures = 0;
        phaseLogCount = 0;
        emergencyLogCount = 0;
        currentCycle = 1;

        for (int i = 0; i < 4; i++)
        {
            laneCounts[i] = 0;
            laneStartId[i] = 0;
            emergencyCounts[i] = 0;

            for (int k = 0; k < VehicleDetector::MAX_EMERGENCY_PER_LANE; k++)
            {
                usedPositions[i][k] = 0;
                emergencyKinds[i][k] = UNKNOWN;
            }
        }

        // Green time = 3 s per vehicle of the longest lane,
        // between 10 and 60 seconds.
        analyzer.setGreenLimits(10, 60);
        greenCorridor.setupDemoNetwork();
    }

    // ---------- BATCH MODE (used by the front end) ----------

    static const char *laneNameOf(int index)
    {
        static const char *names[4] = {"North", "South", "East", "West"};
        return names[index];
    }

    static void printEscaped(const char *text)
    {
        for (; *text != '\0'; text++)
        {
            if (*text == '\\' || *text == '"')
            {
                putchar('\\');
            }

            putchar(*text);
        }
    }

    static void printStrategyJson(const char *key, const StrategyResult &r, bool comma)
    {
        double avg = r.served > 0 ? static_cast<double>(r.totalWait) / r.served : 0.0;
        double avgEmergency = r.emergencyServed > 0
                                  ? static_cast<double>(r.emergencyWait) / r.emergencyServed
                                  : 0.0;
        const char *kindKeys[3] = {"ambulance", "fireTruck", "policeCar"};

        printf("    \"%s\": {\"served\": %d, \"avgWait\": %.2f, \"maxWait\": %d, "
               "\"emergencyServed\": %d, \"avgEmergencyWait\": %.2f, \"phases\": %d, "
               "\"greenGiven\": %d, \"greenWasted\": %d, \"byType\": {",
               key, r.served, avg, r.maxWait, r.emergencyServed, avgEmergency,
               r.phasesUsed, r.greenGiven, r.greenWasted);

        for (int k = 0; k < 3; k++)
        {
            double kindAvg = r.kindServed[k] > 0
                                 ? static_cast<double>(r.kindWait[k]) / r.kindServed[k]
                                 : 0.0;
            printf("\"%s\": {\"served\": %d, \"avgWait\": %.2f}%s", kindKeys[k],
                   r.kindServed[k], kindAvg, k < 2 ? ", " : "");
        }

        printf("}}%s\n", comma ? "," : "");
    }

    void printJsonReport(const char *fileName, int cycles) const
    {
        printf("{\n  \"ok\": true,\n  \"source\": \"");
        printEscaped(fileName);
        printf("\",\n");

        printf("  \"lanes\": [\n");
        for (int lane = 0; lane < 4; lane++)
        {
            int normal = laneCounts[lane] - emergencyCounts[lane];
            int kindCount[3] = {0, 0, 0};

            for (int k = 0; k < emergencyCounts[lane]; k++)
            {
                kindCount[emergencyKinds[lane][k] - AMBULANCE]++;
            }

            printf("    {\"name\": \"%s\", \"detected\": %d, \"emergency\": %d, \"normal\": %d, "
                   "\"ambulances\": %d, \"fireTrucks\": %d, \"policeCars\": %d}%s\n",
                   laneNameOf(lane), laneCounts[lane], emergencyCounts[lane], normal,
                   kindCount[0], kindCount[1], kindCount[2], lane < 3 ? "," : "");
        }
        printf("  ],\n");

        printf("  \"emergencies\": [\n");
        for (int i = 0; i < emergencyLogCount; i++)
        {
            const EmergencyLog &e = emergencyLog[i];
            printf("    {\"id\": %d, \"type\": \"%s\", \"priority\": %d, \"lane\": \"%s\", "
                   "\"destination\": \"%s\", \"distance\": %d, \"serviceTime\": %d, "
                   "\"start\": %d, \"end\": %d, \"eta\": %d, \"route\": [",
                   e.id, Vehicle::typeName(static_cast<VehicleType>(e.type)), e.priority,
                   laneNameOf(e.lane), greenCorridor.getName(e.destination), e.distance,
                   e.serviceTime, e.start, e.end, e.eta);

            for (int k = 0; k < e.routeLength; k++)
            {
                printf("\"%s\"%s", greenCorridor.getName(e.route[k]),
                       k < e.routeLength - 1 ? ", " : "");
            }

            printf("]}%s\n", i < emergencyLogCount - 1 ? "," : "");
        }
        printf("  ],\n");

        printf("  \"phases\": [\n");
        for (int i = 0; i < phaseLogCount; i++)
        {
            const PhaseLog &p = phaseLog[i];
            printf("    {\"cycle\": %d, \"direction\": \"%s\", \"green\": %d, \"start\": %d, "
                   "\"lanes\": [{\"name\": \"%s\", \"served\": %d}, {\"name\": \"%s\", \"served\": %d}]}%s\n",
                   p.cycle, p.phase == NORTH_SOUTH_PHASE ? "NORTH-SOUTH" : "EAST-WEST",
                   p.green, p.start, laneNameOf(p.laneA - 1), p.servedA,
                   laneNameOf(p.laneB - 1), p.servedB, i < phaseLogCount - 1 ? "," : "");
        }
        printf("  ],\n");

        int served = analytics.getNormalServed() + analytics.getEmergencyServed();
        printf("  \"summary\": {\"totalVehicles\": %d, \"served\": %d, \"normalServed\": %d, "
               "\"emergencyServed\": %d, \"remaining\": %d, \"routeFailures\": %d, \"cycles\": %d, "
               "\"avgWait\": %.2f, \"minWait\": %d, \"maxWait\": %d},\n",
               totalInitialVehicles, served, analytics.getNormalServed(),
               analytics.getEmergencyServed(),
               normalRemaining() + emergencyManager.getCount(), emergencyRouteFailures, cycles,
               analytics.averageWaitingTime(), analytics.getMinWait(), analytics.getMaxWait());

        printf("  \"comparison\": {\n    \"fixedGreen\": %d,\n    \"fixedLane\": %s,\n", comparison.getFixedGreen(),
               comparison.getFixedHasLane() ? "true" : "false");
        printStrategyJson("fixed", comparison.getFixedResult(), true);
        printStrategyJson("adaptive", comparison.getAdaptiveResult(), false);
        printf("  },\n");

        printf("  \"history\": [\n");
        for (int i = 0; i < analytics.getRecordCount(); i++)
        {
            const ServiceRecord &r = analytics.getRecord(i);
            printf("    {\"id\": %d, \"lane\": \"%s\", \"type\": \"%s\", \"serviceTime\": %d, "
                   "\"wait\": %d, \"emergency\": %s}%s\n",
                   r.vehicleId, laneCodeName(r.laneCode), Vehicle::typeName(r.type),
                   r.serviceTime, r.waitingTime, r.emergency ? "true" : "false",
                   i < analytics.getRecordCount() - 1 ? "," : "");
        }
        printf("  ]\n}\n");
    }

    // No prompts, console text silenced, ONE JSON report printed at the end.
    int runBatch(const char *fileName, int fixedGreenSeconds, bool fixedLane)
    {
        NullBuffer silent;
        streambuf *original = cout.rdbuf(&silent);

        bool loaded = detector.useInputFile(fileName);

        if (!loaded)
        {
            cout.rdbuf(original);
            printf("{\"ok\": false, \"error\": \"cannot read detections file\"}\n");
            return 1;
        }

        detectTraffic();
        processEmergencyVehicles();

        int cycles = 0;

        while (normalRemaining() > 0 && cycles < 1000)
        {
            cycles++;
            currentCycle = cycles;
            runSignalCycle();
        }

        comparison.setTraffic(laneCounts, emergencyCounts, usedPositions, emergencyKinds);
        comparison.evaluate(greenCorridor, fixedGreenSeconds, fixedLane);

        cout.rdbuf(original);
        printJsonReport(fileName, cycles);
        return 0;
    }

    void run()
    {
        cout << "\n=== SMART TRAFFIC LIGHT OPTIMISER ===\n\n";

        detector.chooseMode();
        detectTraffic();

        displayInitialQueues();
        processEmergencyVehicles();

        int cycle = 1;

        while (true)
        {
            if (cycle > 1)
            {
                cout << "\n=== SIGNAL CYCLE " << cycle << " ===\n";
            }

            runSignalCycle();
            displayFinalQueues();
            displaySummary();

            if (normalRemaining() == 0)
            {
                cout << "\nAll normal vehicles have been served.\n";
                break;
            }

            if (!readYesNo("\nRun another signal cycle?"))
            {
                break;
            }

            cycle++;
        }

        if (readYesNo("\nCompare Fixed-timer vs Adaptive control (Module 6)?"))
        {
            comparison.setTraffic(laneCounts, emergencyCounts, usedPositions, emergencyKinds);
            comparison.compare(greenCorridor);
        }

        if (readYesNo("\nShow detailed analytics (Module 5)?"))
        {
            detailedAnalytics();
        }

        cout << "\n=== SIMULATION COMPLETED ===\n";
    }
};

// MAIN FUNCTION

int main(int argc, char *argv[])
{
    // Front end mode:  traffic --batch detections.txt [fixedGreenSeconds] [fixedRoadHasLane 0|1]
    if (argc > 2 && strcmp(argv[1], "--batch") == 0)
    {
        int fixedGreen = (argc > 3) ? atoi(argv[3]) : 30;
        bool fixedLane = (argc > 4) && atoi(argv[4]) != 0; // 1 = the fixed road also has the green corridor lane
        TrafficSimulation simulation;
        return simulation.runBatch(argv[2], fixedGreen, fixedLane);
    }

    if (argc > 1)
    {
        inputFileName = argv[1];
    }

    do
    {
        TrafficSimulation simulation;
        simulation.run();
    } while (readYesNo("\nRun another simulation?"));

    return 0;
}
