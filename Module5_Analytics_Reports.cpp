// STUDENT 5 — ANALYTICS & REPORTS
// Dynamic service history, linear search, bubble sort, waiting-time statistics.
#include "Common.h"

// ============================================================
// MODULE 6: SERVICE HISTORY AND PERFORMANCE ANALYTICS
// MANUALLY RESIZED DYNAMIC ARRAY
// ============================================================

struct ServiceRecord {
    int vehicleId;
    int laneCode;      // 0 = emergency corridor, 1-4 = N, S, E, W
    int serviceTime;
    int waitingTime;
    bool emergency;
};

const char* laneCodeName(int code) {
    if (code == 0) return "Corridor";
    if (code == 1) return "North";
    if (code == 2) return "South";
    if (code == 3) return "East";
    if (code == 4) return "West";
    return "Unknown";
}

class PerformanceAnalytics : public Reportable {
private:
    ServiceRecord* records;
    int recordCount;
    int capacity;

    int normalVehiclesServed;
    int emergencyVehiclesServed;

    int totalWaitingTime;
    int maximumWaitingTime;
    int minimumWaitingTime;

    // Doubles the array size when it is full.
    bool resize(int newCapacity) {
        if (newCapacity <= capacity) {
            return false;
        }

        ServiceRecord* newRecords = new (nothrow) ServiceRecord[newCapacity];

        if (newRecords == NULL) {
            return false;
        }

        for (int i = 0; i < recordCount; i++) {
            newRecords[i] = records[i];
        }

        delete[] records;
        records = newRecords;
        capacity = newCapacity;

        return true;
    }

public:
    PerformanceAnalytics() {
        capacity = 10;
        recordCount = 0;
        normalVehiclesServed = 0;
        emergencyVehiclesServed = 0;
        totalWaitingTime = 0;
        maximumWaitingTime = 0;
        minimumWaitingTime = 0;

        records = new ServiceRecord[capacity];
    }

    ~PerformanceAnalytics() {
        delete[] records;
    }

    // Owns raw memory, so copying is forbidden (Rule of Three).
    PerformanceAnalytics(const PerformanceAnalytics&) = delete;
    PerformanceAnalytics& operator=(const PerformanceAnalytics&) = delete;

    int getRecordCount() const { return recordCount; }
    int getNormalServed() const { return normalVehiclesServed; }
    int getEmergencyServed() const { return emergencyVehiclesServed; }

    bool addRecord(int id, int lane, int serviceTime,
                   int waitTime, bool emergency) {
        if (recordCount == capacity) {
            if (!resize(capacity * 2)) {
                cout << "Unable to expand history storage.\n";
                return false;
            }
        }

        records[recordCount].vehicleId = id;
        records[recordCount].laneCode = lane;
        records[recordCount].serviceTime = serviceTime;
        records[recordCount].waitingTime = waitTime;
        records[recordCount].emergency = emergency;
        recordCount++;

        if (emergency) {
            emergencyVehiclesServed++;
        } else {
            normalVehiclesServed++;
        }

        totalWaitingTime += waitTime;

        if (recordCount == 1) {
            minimumWaitingTime = waitTime;
            maximumWaitingTime = waitTime;
        } else {
            if (waitTime < minimumWaitingTime) minimumWaitingTime = waitTime;
            if (waitTime > maximumWaitingTime) maximumWaitingTime = waitTime;
        }

        return true;
    }

    int linearSearchVehicle(int id) const {
        for (int i = 0; i < recordCount; i++) {
            if (records[i].vehicleId == id) {
                return i;
            }
        }

        return -1;
    }

    double averageWaitingTime() const {
        if (recordCount == 0) {
            return 0.0;
        }

        return static_cast<double>(totalWaitingTime) / recordCount;
    }

    void searchAndDisplayVehicle(int id) const {
        int index = linearSearchVehicle(id);

        if (index == -1) {
            cout << "Vehicle " << id << " not found in service history.\n";
            return;
        }

        cout << "\nVehicle history found:\n";
        cout << "Vehicle ID: " << records[index].vehicleId << '\n';
        cout << "Lane: " << laneCodeName(records[index].laneCode) << '\n';
        cout << "Service time: " << records[index].serviceTime << "s\n";
        cout << "Waiting time: " << records[index].waitingTime << "s\n";
        cout << "Emergency: "
             << (records[index].emergency ? "Yes" : "No") << '\n';
    }

    // Bubble sort on a COPY, so the real history keeps its order.
    void displaySortedHistory() const {
        printHeader("HISTORY SORTED BY WAIT TIME (bubble sort)");

        if (recordCount == 0) {
            cout << "No history to sort.\n";
            return;
        }

        ServiceRecord* sorted = new (nothrow) ServiceRecord[recordCount];

        if (sorted == NULL) {
            cout << "Not enough memory to sort.\n";
            return;
        }

        for (int i = 0; i < recordCount; i++) {
            sorted[i] = records[i];
        }

        for (int i = 0; i < recordCount - 1; i++) {
            bool swapped = false;

            for (int j = 0; j < recordCount - i - 1; j++) {
                if (sorted[j].waitingTime > sorted[j + 1].waitingTime) {
                    swapValues(sorted[j], sorted[j + 1]);
                    swapped = true;
                }
            }

            if (!swapped) {
                break;
            }
        }

        for (int i = 0; i < recordCount; i++) {
            cout << "Vehicle " << setw(5) << sorted[i].vehicleId
                 << " | " << left << setw(9)
                 << laneCodeName(sorted[i].laneCode) << right
                 << " | Wait: " << sorted[i].waitingTime << "s\n";
        }

        delete[] sorted;
    }

    // maxRows = -1 shows every record.
    void displayServiceHistory(int maxRows = -1) const {
        printHeader("SERVICE HISTORY");

        if (recordCount == 0) {
            cout << "No vehicles have been served.\n";
            return;
        }

        cout << left
             << setw(12) << "Vehicle ID"
             << setw(12) << "Lane"
             << setw(14) << "Service Time"
             << setw(12) << "Wait Time"
             << setw(10) << "Emergency"
             << right << '\n';

        int rows = recordCount;

        if (maxRows >= 0 && maxRows < recordCount) {
            rows = maxRows;
        }

        for (int i = 0; i < rows; i++) {
            cout << left
                 << setw(12) << records[i].vehicleId
                 << setw(12) << laneCodeName(records[i].laneCode)
                 << setw(14) << records[i].serviceTime
                 << setw(12) << records[i].waitingTime
                 << setw(10) << (records[i].emergency ? "Yes" : "No")
                 << right << '\n';
        }

        if (rows < recordCount) {
            cout << "... " << recordCount - rows << " more records hidden.\n";
        }
    }

    void displayReport() const {
        cout << "Normal vehicles served: " << normalVehiclesServed << '\n';
        cout << "Emergency vehicles served: "
             << emergencyVehiclesServed << '\n';
        cout << "Total vehicles served: " << recordCount << '\n';
        cout << "Average waiting time: " << fixed << setprecision(2)
             << averageWaitingTime() << " seconds\n";

        if (recordCount > 0) {
            cout << "Minimum wait: " << minimumWaitingTime << " seconds\n";
            cout << "Maximum wait: " << maximumWaitingTime << " seconds\n";
        }
    }

    const char* moduleName() const {
        return "Module 6 - Analytics";
    }
};

// PENDING (Phase III): merge sort + sorting comparison, binary search,
// median / 90th percentile wait, per-lane statistics.
