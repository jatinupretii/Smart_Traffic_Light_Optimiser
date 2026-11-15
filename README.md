# Smart Traffic Light Optimiser

An adaptive traffic signal that sets the **green time from the number of vehicles waiting in each lane**,
with a dedicated **green corridor lane** and top priority for emergency vehicles.

## The idea

| | Fixed timer | Adaptive system (this project) |
|---|---|---|
| Green time | the same for every direction | 3 s × the longest queue of the direction, kept between 10 s and 60 s |
| Order | always North–South, then East–West | the busier direction first |
| Empty direction | still gets its green | skipped |
| Emergency vehicle | waits inside the normal queue | uses the green corridor lane, gets the highest priority: all signals turn red and only the emergency vehicle passes |

Emergency vehicles are an optional extension. Their priority order is ambulance (100), fire truck (90), police car (70),
decided by a max-heap. A vehicle needs 3 s to cross the stop line and the junction is released 2 s later. The rest of its route
(found with BFS on a small road graph) is treated as a green hold at the next junctions and is not simulated.

## How the parts connect

```
video / image -> Python (YOLO, app.py) -> detections.txt -> C++ (main.cpp, --batch) -> JSON report
              -> Python (traffic_core.py, server.py) -> web page / dashboard
```

Python and C++ are separate programs. Python starts the C++ program and reads the JSON it prints
(`traffic_backend --batch detections.txt <fixed green seconds> <0|1>`).

## Data structures and OOP in `main.cpp`

- **Queue** (circular array) for every lane, **linked list** for the signal event log, **binary max-heap** for emergency priority,
  **graph + BFS** for the green corridor route, **dynamic array**, **linear search** and **bubble sort** for the analytics.
- Abstract class with polymorphism (`Reportable`), inheritance, encapsulation, templates, static members and operator overloading.
- `StrategyComparison` replays the same traffic under the fixed timer and the adaptive system to compare them.

## Run it

You need Python 3 and `g++`. Nothing else has to be installed for the simulation page.

```
python server.py
```

Open `http://localhost:8000` (it opens by itself). Enter the vehicles for each lane, press **Start**, and compare the two roads.
The badge at the bottom says whether the numbers were checked against the real C++ program.

C++ on its own (in PowerShell the `.\` is needed):

```
g++ main.cpp -o traffic_backend.exe
.\traffic_backend.exe --batch detections.txt 30 0
.\traffic_backend.exe            # interactive console version
```

Streamlit dashboard with YOLO video detection (optional, Python 3.12 recommended):

```
python -m pip install -r requirements.txt
streamlit run app.py
```

## Files

| File | Purpose |
|---|---|
| `main.cpp` | C++ backend: lanes, signals, adaptive timing, emergency heap, green corridor, analytics, comparison, `--batch` JSON mode |
| `server.py` | local server: serves the page and runs the C++ program for every traffic |
| `traffic_core.py` | Python helpers: YOLO lane assignment, compiling and running the C++ program |
| `simulation.html` | simple page: enter vehicles, watch the simulation, read the result |
| `app.py` | Streamlit dashboard (YOLO detection + C++ backend) |
| `detections.txt` | sample traffic, one line per lane, vehicles listed from the stop line backwards |

`detections.txt` format: `North: car bus ambulance motorbike truck firetruck police ...`

## Limitations

- The model is a **snapshot**: every detected vehicle is already waiting at t = 0 and no new vehicles arrive.
- Traffic is **simulated**, not measured on a real road. The passing time (3 s per vehicle) and the green limits are assumptions.
- The standard YOLO model has no ambulance, fire truck or police class; those need a custom-trained model.
  Destinations of fire trucks and police cars are demo assumptions.
- With emergency vehicles in the traffic, the adaptive system's **average wait can be higher** than the fixed timer's,
  because the junction is held red for each emergency vehicle. Its emergency vehicles wait far less.
- `simulation.html` contains a JavaScript copy of the C++ rules for the animation. With `server.py` running, each result is compared
  with the real C++ program; the two were also compared on randomly generated traffic during development.

## History

The Phase II backend is kept in the git history under the tag `phase-2-backend`.
