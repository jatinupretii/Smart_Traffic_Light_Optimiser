# Smart Traffic Light Optimiser

An adaptive traffic signal that sets the **green time from the number of vehicles waiting in each lane**,
with a dedicated **green corridor lane** and top priority for emergency vehicles.
A web simulation shows a normal fixed-timer road and the adaptive road side by side for the same traffic.

## The idea

| | Fixed timer | Adaptive system (this project) |
|---|---|---|
| Green time | the same for every direction | 3 s x the longest queue of the direction, kept between 10 s and 60 s |
| Order | always North-South, then East-West | the busier direction first |
| Empty direction | still gets its green | skipped |
| Emergency vehicle | waits inside the normal queue | uses the green corridor lane and gets the highest priority: all signals turn red and only the emergency vehicle passes |

Emergency vehicles are an optional extension. Their priority order is ambulance (100), fire truck (90), police car (70),
decided by a max-heap. A vehicle needs 3 s to cross the stop line and the junction is released 2 s later. The rest of its route
(found with BFS on a small road graph) is treated as a green hold at the next junctions and is not simulated.

## How it works

```
vehicles per lane (typed in the web page or the console, or read from a text file)
        -> C++ backend (main.cpp) -> JSON report -> Python (server.py) -> web simulation
```

Python and C++ are separate programs. Python starts the C++ program and reads the JSON it prints
(`traffic_backend --batch detections.txt <fixed green seconds> <0|1>`).

**Input.** For every lane (North, South, East, West) you give the number of vehicles waiting and, optionally, one emergency vehicle
(ambulance, fire truck or police car). The same traffic can also be written in a text file, one line per lane, vehicles listed from the
stop line backwards. `detections.txt` is a sample:

```
North: car bus car ambulance motorbike truck
South: motorbike car police truck car
```

## Data structures and OOP in `main.cpp`

- **Queue** (circular array) for every lane, **linked list** for the signal event log, **binary max-heap** for emergency priority,
  **graph + BFS** for the green corridor route, **dynamic array**, **linear search** and **bubble sort** for the analytics.
- Abstract class with polymorphism (`Reportable`), inheritance, encapsulation, templates, static members and operator overloading.
- `StrategyComparison` replays the same traffic under the fixed timer and the adaptive system to compare them.

## Run it

You need Python 3 and `g++`. Nothing else has to be installed.

**Web simulation**

```
python server.py
```

Open `http://localhost:8000` (it opens by itself). Enter the vehicles for each lane, press **Start**, and compare the two roads.
The badge at the bottom says whether the numbers were checked against the real C++ program.
If port 8000 is busy, use `python server.py --port 8001`.

**C++ console program** (in PowerShell the `.\` is needed)

```
g++ main.cpp -o traffic_backend.exe
.\traffic_backend.exe                              # interactive console version
.\traffic_backend.exe --batch detections.txt 30 0  # one JSON report (fixed green 30 s, fixed road without a lane)
```

## Files

| File | Purpose |
|---|---|
| `main.cpp` | C++ backend: lanes, signals, adaptive timing, emergency heap, green corridor, analytics, comparison, `--batch` JSON mode |
| `server.py` | local server: serves the page and runs the C++ program for every traffic |
| `traffic_core.py` | Python helper that compiles and runs the C++ program (it also holds optional helper code used only by `app.py`) |
| `simulation.html` | the web simulation: enter vehicles, watch both roads, read the result |
| `detections.txt` | sample traffic file |
| `app.py`, `requirements.txt` | optional Streamlit dashboard, not needed for the simulation (needs `pip install -r requirements.txt`) |

## Limitations

- The model is a **snapshot**: every vehicle is already waiting at t = 0 and no new vehicles arrive.
- Traffic is **simulated**, not measured on a real road. The passing time (3 s per vehicle) and the green limits are assumptions.
- Vehicle counts are **entered by the user**. This project does not detect vehicles from a camera, video or photo.
  `app.py` contains an optional dashboard with detection code that was not tested on real footage.
- Destinations of the emergency vehicles (hospital, incident sites) are demo assumptions.
- With emergency vehicles in the traffic, the adaptive system's **average wait can be higher** than the fixed timer's,
  because the junction is held red for each emergency vehicle. Its emergency vehicles wait far less.
- `simulation.html` contains a JavaScript copy of the C++ rules for the animation. With `server.py` running, each result is compared
  with the real C++ program; the two were also compared on randomly generated traffic during development.
