// STUDENT 3: Traffic signal states and phase controller
#include "Common.h"

// ============================================================
// MODULE 2: TRAFFIC SIGNAL STATE MACHINE
// ============================================================

enum SignalState {
    RED,
    YELLOW,
    GREEN
};

const char* signalStateText(SignalState state) {
    if (state == RED) return "RED";
    if (state == YELLOW) return "YELLOW";
    return "GREEN";
}

class TrafficSignal {
private:
    char signalName[30];
    SignalState currentState;
    int stateChangeCount;

public:
    TrafficSignal() {
        strcpy(signalName, "Unknown Signal");
        currentState = RED;
        stateChangeCount = 0;
    }

    TrafficSignal(const char* name) {
        strncpy(signalName, name, sizeof(signalName) - 1);
        signalName[sizeof(signalName) - 1] = '\0';
        currentState = RED;
        stateChangeCount = 0;
    }

    SignalState getState() const {
        return currentState;
    }

    const char* getName() const {
        return signalName;
    }

    const char* getStateText() const {
        return signalStateText(currentState);
    }

    int getStateChangeCount() const {
        return stateChangeCount;
    }

    bool changeState(SignalState newState) {
        if (newState == currentState)
            return true;

        bool valid =
            (currentState == RED && newState == GREEN) ||
            (currentState == GREEN && newState == YELLOW) ||
            (currentState == YELLOW && newState == RED);

        if (!valid) {
            cout << "Invalid transition for " << signalName << ": "
                 << signalStateText(currentState) << " -> "
                 << signalStateText(newState)
                 << " is not allowed.\n";
            return false;
        }

        currentState = newState;
        stateChangeCount++;
        return true;
    }
};

// ============================================================
// SINGLY LINKED LIST: SIGNAL EVENT LOG
// ============================================================

class SignalEventLog {
private:
    struct Node {
        int eventNumber;
        char signalName[30];
        SignalState oldState;
        SignalState newState;
        Node* next;

        Node(int number, const char* name,
             SignalState oldValue, SignalState newValue) {
            eventNumber = number;

            strncpy(signalName, name, sizeof(signalName) - 1);
            signalName[sizeof(signalName) - 1] = '\0';

            oldState = oldValue;
            newState = newValue;
            next = NULL;
        }
    };

    Node* head;
    Node* tail;
    int eventCount;

public:
    SignalEventLog() : head(NULL), tail(NULL), eventCount(0) {}

    SignalEventLog(const SignalEventLog&) = delete;
    SignalEventLog& operator=(const SignalEventLog&) = delete;

    ~SignalEventLog() {
        Node* current = head;

        while (current != NULL) {
            Node* nextNode = current->next;
            delete current;
            current = nextNode;
        }
    }

    void addEvent(const char* name,
                  SignalState oldState,
                  SignalState newState) {
        Node* newNode =
            new Node(eventCount + 1, name, oldState, newState);

        if (head == NULL) {
            head = newNode;
            tail = newNode;
        } else {
            tail->next = newNode;
            tail = newNode;
        }

        eventCount++;
    }

    int size() const {
        return eventCount;
    }

    void display() const {
        cout << "\nSignal Event Log (linked list):\n";

        if (head == NULL) {
            cout << "No signal transitions recorded.\n";
            return;
        }

        const Node* current = head;

        while (current != NULL) {
            cout << "  #" << current->eventNumber << "  "
                 << current->signalName << ": "
                 << signalStateText(current->oldState) << " -> "
                 << signalStateText(current->newState) << '\n';

            current = current->next;
        }
    }
};

// ============================================================
// TRAFFIC PHASE
// ============================================================

enum TrafficPhase {
    NORTH_SOUTH_PHASE,
    EAST_WEST_PHASE
};

// ============================================================
// PHASE CONTROLLER
// ============================================================

class PhaseController : public Reportable {
private:
    TrafficSignal northSouthSignal;
    TrafficSignal eastWestSignal;

    TrafficPhase activePhase;
    int phaseSwitches;
    int safetyViolations;

    SignalEventLog eventLog;

    void changeAndLog(TrafficSignal& signal,
                      SignalState newState) {
        SignalState oldState = signal.getState();

        if (oldState == newState)
            return;

        if (signal.changeState(newState)) {
            eventLog.addEvent(signal.getName(),
                              oldState, newState);
        }
    }

    void moveToRed(TrafficSignal& signal) {
        if (signal.getState() == GREEN)
            changeAndLog(signal, YELLOW);

        if (signal.getState() == YELLOW)
            changeAndLog(signal, RED);
    }

public:
    PhaseController()
        : northSouthSignal("North-South Signal"),
          eastWestSignal("East-West Signal") {
        activePhase = NORTH_SOUTH_PHASE;
        phaseSwitches = 0;
        safetyViolations = 0;
    }

    static int transitionTime() {
        return YELLOW_TIME + ALL_RED_TIME;
    }

    void allRed() {
        moveToRed(northSouthSignal);
        moveToRed(eastWestSignal);
    }

    void activatePhase(TrafficPhase phase) {
        allRed();

        if (phase != activePhase)
            phaseSwitches++;

        activePhase = phase;

        if (phase == NORTH_SOUTH_PHASE)
            changeAndLog(northSouthSignal, GREEN);
        else
            changeAndLog(eastWestSignal, GREEN);

        if (!isSafe())
            safetyViolations++;
    }

    void finishPhase() {
        allRed();
    }

    bool isSafe() const {
        return !(northSouthSignal.getState() == GREEN &&
                 eastWestSignal.getState() == GREEN);
    }

    int getPhaseSwitches() const {
        return phaseSwitches;
    }

    void displayStatusLine() const {
        cout << "Signals -> North-South: "
             << northSouthSignal.getStateText()
             << " | East-West: "
             << eastWestSignal.getStateText() << '\n';
    }

    void displayReport() const {
        displayStatusLine();

        cout << "North-South state changes: "
             << northSouthSignal.getStateChangeCount() << '\n';

        cout << "East-West state changes: "
             << eastWestSignal.getStateChangeCount() << '\n';

        cout << "Phase switches: " << phaseSwitches << '\n';

        cout << "Safety violations (both green): "
             << safetyViolations << '\n';

        cout << "Recorded signal events: "
             << eventLog.size() << '\n';

        eventLog.display();
    }

    const char* moduleName() const {
        return "Module 2 - Signal Phase Control";
    }
};

// ============================================================
// ADAPTIVE TRAFFIC TIMING
// ============================================================

class TrafficAnalyzer : public Reportable {
private:
    int secondsPerVehicle;
    int minGreenTime;
    int maxGreenTime;

public:
    TrafficAnalyzer() {
        secondsPerVehicle = 2;
        minGreenTime = 10;
        maxGreenTime = 60;
    }

    int getSecondsPerVehicle() const {
        return secondsPerVehicle;
    }

    void setGreenLimits(int minTime, int maxTime) {
        if (minTime > 0 && maxTime >= minTime) {
            minGreenTime = minTime;
            maxGreenTime = maxTime;
        }
    }

    int calculateGreenTime(int vehicleCount) const {
        if (vehicleCount < 0)
            vehicleCount = 0;

        int greenTime = vehicleCount * secondsPerVehicle;

        if (greenTime < minGreenTime)
            greenTime = minGreenTime;

        if (greenTime > maxGreenTime)
            greenTime = maxGreenTime;

        return greenTime;
    }

    TrafficPhase choosePhase(int northSouthCount,
                             int eastWestCount) const {
        if (northSouthCount >= eastWestCount)
            return NORTH_SOUTH_PHASE;

        return EAST_WEST_PHASE;
    }

    void displayReport() const {
        cout << "\nAdaptive Timing Report\n";
        cout << "Seconds per vehicle: "
             << secondsPerVehicle << '\n';
        cout << "Minimum green time: "
             << minGreenTime << " seconds\n";
        cout << "Maximum green time: "
             << maxGreenTime << " seconds\n";
    }

    const char* moduleName() const {
        return "Module 4 - Adaptive Traffic Timing";
    }
};

// End of Module 2