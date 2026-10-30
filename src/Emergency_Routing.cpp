//Module 4:: Emergency Routing (priority heap + green corridor)

#include "Common.h"

// ============================================================
// MODULE 4 (part 1): EMERGENCY VEHICLE PRIORITY
// MANUAL BINARY MAX HEAP
// ============================================================

class EmergencyManager : public Reportable
{
private:
    Vehicle heap[MAX_EMERGENCIES];
    int heapSize;
    int totalInserted;
    int totalExtracted;

    // Higher priority first; then earlier arrival; then smaller ID.
    bool hasHigherPriority(const Vehicle& a, const Vehicle& b) const
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

    bool insertEmergency(const Vehicle& vehicle)
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

    bool extractHighestPriority(Vehicle& vehicle)
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

    const char* moduleName() const
    {
        return "Module 4 - Emergency Priority";
    }
};

// PENDING (Phase III): change priority of a queued vehicle, heap tree view,
// non-destructive dispatch order.

// ============================================================
// MODULE 4 (part 2): GREEN CORRIDOR
// WEIGHTED GRAPH (ADJACENCY MATRIX) + BFS
// ============================================================

class GreenCorridor : public Reportable
{
private:
    int distanceMap[MAX_INTERSECTIONS][MAX_INTERSECTIONS];   // 0 = no road
    bool blocked[MAX_INTERSECTIONS][MAX_INTERSECTIONS];
    const char* intersectionNames[MAX_INTERSECTIONS];

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

    const char* getName(int id) const
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
    bool findShortestRoute(int source, int destination, int route[], int& routeLength) const
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

    const char* moduleName() const
    {
        return "Module 4 - Green Corridor";
    }
};

// PENDING (Phase III): Dijkstra shortest-distance route, DFS reachability
// check, green-hold marking of corridor intersections, unblock road.