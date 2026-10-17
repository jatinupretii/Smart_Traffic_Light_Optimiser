# Smart Traffic Light Optimiser
## Six-Module Development Plan

### Project Rules
- Language: C++14.
- No STL containers or STL algorithms.
- Data structures must be implemented manually.
- Each module must have its own implementation and test cases.
- Modules must use agreed interfaces to support integration.
- No module is considered complete until its code is tested and explainable in the viva.

## Part A — Object-Oriented Programming

### Module O1: Vehicle Management
**Owner:** Member 1

Responsibilities:
- Implement vehicle classes and vehicle information.
- Store vehicle ID, vehicle type, lane information, waiting time and emergency status.
- Apply encapsulation and constructor-based initialization.
- Use inheritance or polymorphism only where they add meaningful behaviour.
- Validate vehicle data.

Deliverables:
- Vehicle class declarations and implementations.
- Vehicle validation tests.
- OOP viva notes explaining encapsulation, constructors and any inheritance used.

### Module O2: Traffic Signal Management
**Owner:** Member 2

Responsibilities:
- Implement the traffic signal state model.
- Support RED, GREEN and YELLOW states.
- Validate signal timing and state transitions.
- Prevent conflicting traffic directions from receiving GREEN simultaneously.
- Handle emergency-related signal overrides safely.

Deliverables:
- Signal class declarations and implementations.
- Signal transition tests.
- OOP viva notes explaining class design, encapsulation and object interaction.

### Module O3: Intersection and Traffic Controller
**Owner:** Member 3

Responsibilities:
- Represent an intersection and its lanes/signals.
- Coordinate normal traffic phases.
- Connect the vehicle, queue and signal modules.
- Produce simulation summaries and queue reports.
- Integrate the complete application.

Deliverables:
- Intersection and controller classes.
- Integration tests.
- OOP viva notes explaining composition, abstraction and modular design.

## Part B — Data Structures and Algorithms

### Module D1: Manual Circular Queue
**Owner:** Member 4

Responsibilities:
- Implement an array-based circular queue without std::queue or std::vector.
- Implement enqueue, dequeue, isEmpty and isFull.
- Handle wrap-around, overflow and underflow.
- Preserve FIFO order.
- Integrate the queue with lane traffic.

Deliverables:
- VehicleQueue class.
- Tests for FIFO order, empty/full conditions and wrap-around.
- Time and space complexity analysis.

### Module D2: Manual Priority Queue / Binary Heap
**Owner:** Member 5

Responsibilities:
- Implement a binary heap using an array.
- Implement insertion, highest-priority extraction and heap restoration.
- Define deterministic emergency priority and tie-breaking rules.
- Connect emergency vehicle selection to the Green Corridor.
- Test the heap property after operations.

Deliverables:
- EmergencyHeap class.
- Tests for priority ordering, ties and empty-heap handling.
- Time and space complexity analysis.

### Module D3: Graph and Green Corridor Routing
**Owner:** Member 6

Responsibilities:
- Represent intersections as vertices and roads as edges.
- Implement a graph using a manually managed adjacency list or adjacency matrix.
- Implement BFS without STL containers.
- Reconstruct and display a route between intersections.
- Handle disconnected intersections and missing routes.

Deliverables:
- TrafficGraph class.
- Tests for connected, disconnected and alternative routes.
- Time and space complexity analysis.

## Integration Rules
1. Agree on class interfaces before implementing dependent modules.
2. The circular queue must be usable by the lane/intersection system.
3. The emergency heap must use the same vehicle information and priority rules agreed by the team.
4. The graph must provide a route that the controller can use to coordinate signals.
5. Only the controller should coordinate conflicting signal phases.
6. Every module must be tested independently before integration.
7. Keep compiled executables out of Git commits.

## Definition of Done
A module is complete when:
- Its source code is committed to its assigned branch.
- It builds with the project's C++14 compiler.
- Its test cases pass.
- Its owner can explain its logic and complexity where applicable.
- Its interface is documented for the integration owner.