// Shared declarations and helper functions for the Smart Traffic Light Optimiser.
#ifndef COMMON_H
#define COMMON_H
#include <iostream>
#include <iomanip>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <new>
using namespace std;

const int SECONDS_PER_VEHICLE = 3;
const int YELLOW_TIME = 3;
const int ALL_RED_TIME = 2;
const int LANE_CAPACITY = 100;
const int MAX_EMERGENCIES = 100;
const int MAX_INTERSECTIONS = 10;

template <typename T>
void swapValues(T& a, T& b) { T temp = a; a = b; b = temp; }

void printHeader(const char* title) {
    cout << "\n============================================================\n";
    cout << "  " << title << '\n';
    cout << "============================================================\n";
}

int readInt(const char* prompt, int minValue, int maxValue) {
    int value;
    while (true) {
        cout << prompt;
        if (cin >> value) {
            if (value >= minValue && value <= maxValue) return value;
        } else if (cin.eof()) {
            cout << "\\nInput ended. Exiting program.\\n";
            exit(0);
        }
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Invalid input. Enter a number from " << minValue
             << " to " << maxValue << ".\\n";
    }
}

class Reportable {
public:
    virtual ~Reportable() {}
    virtual void displayReport() const = 0;
    virtual const char* moduleName() const = 0;
};
#endif
