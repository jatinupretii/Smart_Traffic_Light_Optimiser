// STUDENT 2 — SIGNAL CONTROL & ADAPTIVE TIMING
// Signal state machine, phase controller, density-based phase selection, green limits.
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

    SignalState getState() const { return currentState; }
    const char* getName() const { return signalName; }
    const char* getStateText() const { return signalStateText(currentState); }
    int getStateChangeCount() const { return stateChangeCount; }

    // Strict cycle: RED -> GREEN -> YELLOW -> RED.
    bool changeState(SignalState newState) {
        if (newState == currentState) {
            return true;
        }

        bool valid =
            (currentState == RED    && newState == GREEN)  ||
            (currentState == GREEN  && newState == YELLOW) ||
            (currentState == YELLOW && newState == RED);

        if (!valid) {
            cout << "Invalid transition for " << signalName << ": "
                 << signalStateText(currentState) << " -> "
                 << signalStateText(newState) << " is not allowed.\n";
            return false;
        }

        currentState = newState;
        stateChangeCount++;
        return true;
    }
};


// ============================================================
// MODULE 2: PHASE CONTROLLER
// ============================================================

enum TrafficPhase {
    NORTH_SOUTH_PHASE,
    EAST_WEST_PHASE
};

class PhaseController : public Reportable {
private:
    TrafficSignal northSouthSignal;
    TrafficSignal eastWestSignal;

    TrafficPhase activePhase;
    int phaseSwitches;
    int safetyViolations;

    void moveToRed(TrafficSignal& signal) {
        if (signal.getState() == GREEN) {
            signal.changeState(YELLOW);
        }

        if (signal.getState() == YELLOW) {
            signal.changeState(RED);
        }
    }

public:
    PhaseController()
        : northSouthSignal("North-South Signal"),
          eastWestSignal("East-West Signal") {
        activePhase = NORTH_SOUTH_PHASE;
        phaseSwitches = 0;
        safetyViolations = 0;
    }

    // Time lost between two green phases.
    static int transitionTime() {
        return YELLOW_TIME + ALL_RED_TIME;
    }

    void allRed() {
        moveToRed(northSouthSignal);
        moveToRed(eastWestSignal);
    }

    void activatePhase(TrafficPhase phase) {
        // Never allow one direction green while the other is green.
        allRed();

        if (phase != activePhase) {
            phaseSwitches++;
        }

        activePhase = phase;

        if (phase == NORTH_SOUTH_PHASE) {
            northSouthSignal.changeState(GREEN);
        } else {
            eastWestSignal.changeState(GREEN);
        }

        if (!isSafe()) {
            safetyViolations++;
        }
    }

    void finishPhase() {
        allRed();
    }

    bool isSafe() const {
        return !(northSouthSignal.getState() == GREEN &&
                 eastWestSignal.getState() == GREEN);
    }

    int getPhaseSwitches() const { return phaseSwitches; }

    void displayStatusLine() const {
        cout << "Signals -> North-South: " << northSouthSignal.getStateText()
             << " | East-West: " << eastWestSignal.getStateText() << '\n';
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
    }

    const char* moduleName() const {
        return "Module 2 - Signal Phase Control";
    }
};

// PENDING (Phase III): signal event log (linked list), emergency override
// mode that holds every signal red.

// ============================================================
// MODULE 3: ADAPTIVE TRAFFIC OPTIMISATION
// ============================================================

class TrafficAnalyzer : public Reportable {
private:
    int minimumGreenTime;
    int maximumGreenTime;
    int secondsPerVehicle;

public:
    TrafficAnalyzer() {
        minimumGreenTime = 10;
        maximumGreenTime = 40;
        secondsPerVehicle = SECONDS_PER_VEHICLE;
    }

    void setGreenLimits(int minimum, int maximum) {
        if (minimum < 1 || maximum < minimum) {
            cout << "Invalid green-time limits.\n";
            return;
        }

        minimumGreenTime = minimum;
        maximumGreenTime = maximum;
    }

    int getSecondsPerVehicle() const { return secondsPerVehicle; }

    // Green time grows with the queue but stays between the limits.
    int calculateGreenTime(int vehicleCount) const {
        int calculatedTime = vehicleCount * secondsPerVehicle;

        if (calculatedTime < minimumGreenTime) {
            calculatedTime = minimumGreenTime;
        }

        if (calculatedTime > maximumGreenTime) {
            calculatedTime = maximumGreenTime;
        }

        return calculatedTime;
    }

    // The heavier direction goes first.
    TrafficPhase choosePhase(int northSouthCount, int eastWestCount) const {
        if (northSouthCount >= eastWestCount) {
            return NORTH_SOUTH_PHASE;
        }

        return EAST_WEST_PHASE;
    }

    void displayAnalysis(int north, int south, int east, int west) const {
        int ns = north + south;
        int ew = east + west;

        printHeader("ADAPTIVE TRAFFIC ANALYSIS");

        cout << "North-South vehicles: " << ns << '\n';
        cout << "East-West vehicles:   " << ew << '\n';
        cout << "North-South green time: "
             << calculateGreenTime(ns) << " seconds\n";
        cout << "East-West green time:   "
             << calculateGreenTime(ew) << " seconds\n";

        if (ns > ew) {
            cout << "Higher traffic density: North-South\n";
        } else if (ew > ns) {
            cout << "Higher traffic density: East-West\n";
        } else {
            cout << "Traffic density is equal in both directions.\n";
        }
    }

    void displayReport() const {
        cout << "Green time limits: " << minimumGreenTime
             << "s to " << maximumGreenTime << "s\n";
        cout << "Seconds per vehicle: " << secondsPerVehicle << '\n';
    }

    const char* moduleName() const {
        return "Module 3 - Adaptive Optimisation";
    }
};

// PENDING (Phase III): congestion levels, starvation guard, fixed-time vs
// adaptive comparison.
