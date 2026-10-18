// STUDENT 1 — VEHICLE & LANE QUEUES
// Vehicle representation, priority, circular queue, enqueue/dequeue, lane status.
#include "Common.h"

// ============================================================
// MODULE 1: VEHICLE MANAGEMENT
// ============================================================

enum VehicleType {
    CAR,
    BUS,
    MOTORBIKE,
    TRUCK,
    EMERGENCY
};

class Vehicle {
private:
    int id;
    VehicleType type;
    int arrivalTime;
    int priority;

    static int createdCount;   // static member: counts vehicles built

public:
    static bool isEmergencyType(VehicleType t) {
        return t == EMERGENCY;
    }

    // Phase II: all emergency vehicles share one highest priority.
    static int defaultPriority(VehicleType t) {
        return (t == EMERGENCY) ? 100 : 0;
    }

    static const char* typeName(VehicleType t) {
        switch (t) {
            case CAR:        return "Car";
            case BUS:        return "Bus";
            case MOTORBIKE:  return "Motorbike";
            case TRUCK:      return "Truck";
            case EMERGENCY:  return "Emergency Vehicle";
        }
        return "Unknown";
    }

    static int getCreatedCount() {
        return createdCount;
    }

    Vehicle() {
        id = 0;
        type = CAR;
        arrivalTime = 0;
        priority = 0;
    }

    Vehicle(int vehicleId, VehicleType vehicleType,
            int arrival, int emergencyPriority = 0) {
        id = vehicleId;
        type = vehicleType;
        arrivalTime = arrival;

        if (isEmergencyType(type)) {
            priority = (emergencyPriority > 0)
                           ? emergencyPriority
                           : defaultPriority(type);
        } else {
            priority = 0;
        }

        createdCount++;
    }

    int getId() const { return id; }
    VehicleType getType() const { return type; }
    int getArrivalTime() const { return arrivalTime; }
    int getPriority() const { return priority; }
    bool isEmergency() const { return isEmergencyType(type); }

    // Friend function: prints a vehicle with <<.
    friend ostream& operator<<(ostream& out, const Vehicle& v);
};

int Vehicle::createdCount = 0;

ostream& operator<<(ostream& out, const Vehicle& v) {
    out << Vehicle::typeName(v.type) << " #" << v.id
        << " (arrived " << v.arrivalTime << "s)";

    if (v.isEmergency()) {
        out << " priority " << v.priority;
    }

    return out;
}

// PENDING (Phase III): separate Ambulance / Fire Truck / Police Car types,
// vehicle-specific priorities and destinations, random/live arrivals.

// ============================================================
// MODULE 1: LANE USING A CIRCULAR QUEUE
// ============================================================

class Lane : public Reportable {
private:
    Vehicle vehicles[LANE_CAPACITY];

    int frontIndex;
    int rearIndex;
    int vehicleCount;

    int totalArrived;
    int totalServed;
    int totalRejected;

    char laneName[20];

    void initCounters() {
        frontIndex = 0;
        rearIndex = -1;
        vehicleCount = 0;
        totalArrived = 0;
        totalServed = 0;
        totalRejected = 0;
    }

public:
    Lane() {
        initCounters();
        strcpy(laneName, "Unknown");
    }

    Lane(const char* name) {
        initCounters();
        strncpy(laneName, name, sizeof(laneName) - 1);
        laneName[sizeof(laneName) - 1] = '\0';
    }

    bool isEmpty() const { return vehicleCount == 0; }
    bool isFull() const { return vehicleCount == LANE_CAPACITY; }
    int getCount() const { return vehicleCount; }
    const char* getName() const { return laneName; }

    // ENQUEUE
    bool addVehicle(const Vehicle& vehicle) {
        if (isFull()) {
            totalRejected++;
            return false;
        }

        rearIndex = (rearIndex + 1) % LANE_CAPACITY;
        vehicles[rearIndex] = vehicle;
        vehicleCount++;
        totalArrived++;

        return true;
    }

    // DEQUEUE
    bool serveVehicle(Vehicle& servedVehicle) {
        if (isEmpty()) {
            return false;
        }

        servedVehicle = vehicles[frontIndex];
        frontIndex = (frontIndex + 1) % LANE_CAPACITY;
        vehicleCount--;
        totalServed++;

        return true;
    }

    // PEEK
    bool peekVehicle(Vehicle& vehicle) const {
        if (isEmpty()) {
            return false;
        }

        vehicle = vehicles[frontIndex];
        return true;
    }

    // Takes ONE vehicle out of the middle of the queue (used when a siren
    // is detected inside a normal queue). Later vehicles shift forward.
    bool extractVehicleById(int vehicleId, Vehicle& removed) {
        int position = -1;

        for (int i = 0; i < vehicleCount; i++) {
            if (vehicles[(frontIndex + i) % LANE_CAPACITY].getId()
                == vehicleId) {
                position = i;
                break;
            }
        }

        if (position == -1) {
            return false;
        }

        removed = vehicles[(frontIndex + position) % LANE_CAPACITY];

        for (int i = position; i < vehicleCount - 1; i++) {
            vehicles[(frontIndex + i) % LANE_CAPACITY] =
                vehicles[(frontIndex + i + 1) % LANE_CAPACITY];
        }

        rearIndex = (rearIndex - 1 + LANE_CAPACITY) % LANE_CAPACITY;
        vehicleCount--;
        return true;
    }

    void displayStatusBar() const {
        const int BAR_WIDTH = 20;
        int filled = vehicleCount * BAR_WIDTH / LANE_CAPACITY;

        cout << left << setw(7) << laneName << right << " [";

        for (int i = 0; i < BAR_WIDTH; i++) {
            cout << (i < filled ? '#' : '.');
        }

        cout << "] " << setw(3) << vehicleCount
             << "/" << LANE_CAPACITY << '\n';
    }

    void displayQueue() const {
        const int SHOW_LIMIT = 12;

        cout << "\nLane: " << laneName << " | waiting: "
             << vehicleCount << '\n';

        if (isEmpty()) {
            cout << "Queue is empty.\n";
            return;
        }

        cout << "Queue order: ";

        int shown = vehicleCount < SHOW_LIMIT ? vehicleCount : SHOW_LIMIT;

        for (int i = 0; i < shown; i++) {
            cout << vehicles[(frontIndex + i) % LANE_CAPACITY].getId();

            if (i < shown - 1) {
                cout << " -> ";
            }
        }

        if (vehicleCount > SHOW_LIMIT) {
            cout << " -> ... (" << vehicleCount - SHOW_LIMIT << " more)";
        }

        cout << '\n';
    }

    void displayReport() const {
        cout << left << setw(7) << laneName << right
             << " waiting: " << setw(3) << vehicleCount
             << " | arrived: " << setw(3) << totalArrived
             << " | served: " << setw(3) << totalServed
             << " | rejected: " << totalRejected << '\n';
    }

    const char* moduleName() const {
        return "Module 1 - Lane Management";
    }
};

// PENDING (Phase III): remove a vehicle from the middle of the queue,
// peak-queue statistics.


// ============================================================
// MODULE 1 (addition): VEHICLE DETECTOR - SIMULATED SENSOR LAYER
//
// A console program has no camera or road sensor, so detection is
// SIMULATED. In a real junction this class would be replaced by
// real hardware (inductive loop, IR/ultrasonic sensor, camera with
// object detection, siren/RFID receiver). The rest of the project
// would not change: it only needs "how many vehicles per lane" and
// "which vehicle is an emergency vehicle".
//
//   Mode 1 - Manual : the operator types what the sensor reads.
//   Mode 2 - Random : a seeded generator acts as the sensor.
// ============================================================
