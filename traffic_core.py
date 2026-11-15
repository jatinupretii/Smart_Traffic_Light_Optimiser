"""
traffic_core.py - backend, manual traffic input, and report helpers.

Vehicle information is supplied manually through detections.txt or the existing
simulation interface.
Vehicles are entered manually in detections.txt or through the existing
simulation interface. The C++ backend remains responsible for traffic logic.
"""

import json
import os
import shutil
import subprocess

LANES = ["North", "South", "East", "West"]

EMERGENCY_TYPES = [
    ("ambulance", "Ambulance", "🚑"),
    ("fireTruck", "Fire truck", "🚒"),
    ("policeCar", "Police car", "🚓"),
]

DEMO_KINDS = {
    "ambulance": "ambulance",
    "amb": "ambulance",
    "fire": "firetruck",
    "firetruck": "firetruck",
    "police": "police",
}


# ---------------------------------------------------------------------------
# Manual traffic input and emergency vehicle handling
# ---------------------------------------------------------------------------

def apply_demo_emergency(lanes, spec):
    """
    Insert manually specified emergency vehicles into lane queues.

    Example:
        "North:2:fire,East:1:police,West:3"

    The vehicle kind may be ambulance, fire, or police.
    If omitted, ambulance is used.
    """
    messages = []

    for item in spec.split(","):
        item = item.strip()
        if not item:
            continue

        parts = [part.strip() for part in item.split(":")]

        if len(parts) not in (2, 3):
            raise ValueError(
                "Emergency format is Lane:position[:ambulance|fire|police], "
                "for example North:2:fire"
            )

        lane_names = [name.lower() for name in LANES]
        if parts[0].lower() not in lane_names:
            raise ValueError(
                "Unknown lane '%s' (use North, South, East or West)" % parts[0]
            )

        lane = lane_names.index(parts[0].lower())
        kind = "ambulance"

        if len(parts) == 3:
            if parts[2].lower() not in DEMO_KINDS:
                raise ValueError(
                    "Unknown vehicle kind '%s' (use ambulance, fire or police)"
                    % parts[2]
                )
            kind = DEMO_KINDS[parts[2].lower()]

        try:
            position = int(parts[1])
        except ValueError:
            raise ValueError("Emergency vehicle position must be a number")

        index = max(0, min(position - 1, len(lanes[lane])))
        lanes[lane].insert(index, kind)

        messages.append(
            "MANUAL OVERRIDE: %s inserted in %s at position %d"
            % (kind, LANES[lane], index + 1)
        )

    return messages


# Keep the previous function name so existing code using it does not break.
apply_demo_ambulances = apply_demo_emergency


def write_detections(path, lanes):
    """Write the four manually supplied lane queues to detections.txt."""
    with open(path, "w") as file:
        file.write(
            "# lane: vehicles from the stop line (front of queue) to the back\n"
        )
        for name, sequence in zip(LANES, lanes):
            file.write(name + ": " + " ".join(sequence) + "\n")


def read_detections(path):
    """Read detections.txt into four lane lists."""
    lanes = [[] for _ in range(4)]

    with open(path) as file:
        for line in file:
            line = line.strip()

            if not line or line.startswith("#") or ":" not in line:
                continue

            name, rest = line.split(":", 1)
            lane_names = [lane.lower() for lane in LANES]

            if name.strip().lower() in lane_names:
                lane_index = lane_names.index(name.strip().lower())
                lanes[lane_index] = rest.split()

    return lanes


# ---------------------------------------------------------------------------
# C++ backend
# ---------------------------------------------------------------------------

def find_backend_source(folder):
    for name in ["main.cpp", "smart_traffic_optimiser.cpp"]:
        path = os.path.join(folder, name)
        if os.path.exists(path):
            return path
    return ""


def backend_exe_path(folder):
    executable_name = (
        "traffic_backend.exe" if os.name == "nt" else "traffic_backend"
    )
    return os.path.join(folder, executable_name)


def ensure_backend(cpp, exe):
    """
    Compile the C++ backend when it is missing or older than main.cpp.
    Returns (success, message).
    """
    if not cpp or not os.path.exists(cpp):
        return (
            False,
            "C++ source not found. Put main.cpp in the same folder as the Python files."
        )

    if os.path.exists(exe) and os.path.getmtime(exe) >= os.path.getmtime(cpp):
        return True, "Backend is up to date."

    compiler = shutil.which("g++")
    if not compiler:
        return (
            False,
            "g++ was not found. Install a C++ compiler, or compile by hand:\n"
            + "g++ main.cpp -o " + os.path.basename(exe)
        )

    result = subprocess.run(
        [compiler, "-std=c++11", "-O2", cpp, "-o", exe],
        capture_output=True,
        text=True
    )

    if result.returncode != 0:
        return False, "C++ compile failed:\n" + result.stderr[-1500:]

    return True, "Backend compiled."


def run_backend(exe, detections_path, fixed_green, fixed_lane=False):
    """
    Run the C++ backend in batch mode and return its JSON report.
    fixed_lane=True also gives the fixed-timer road the green corridor lane.
    """
    result = subprocess.run(
        [
            exe,
            "--batch",
            detections_path,
            str(int(fixed_green)),
            "1" if fixed_lane else "0"
        ],
        capture_output=True,
        text=True,
        timeout=60
    )

    try:
        data = json.loads(result.stdout)
    except ValueError:
        raise RuntimeError(
            "Backend did not return a JSON report.\n"
            + result.stdout[-500:]
            + result.stderr[-500:]
        )

    if not data.get("ok"):
        raise RuntimeError(data.get("error", "Backend reported an error"))

    return data


# ---------------------------------------------------------------------------
# Report helpers
# ---------------------------------------------------------------------------

def timeline_html(data):
    """
    Build the signal timeline:
    red = junction held for an emergency vehicle,
    teal = North-South green,
    amber = East-West green.
    """
    segments = []

    for emergency in data["emergencies"]:
        segments.append((
            "Corridor",
            emergency.get("start", 0),
            emergency.get("end", emergency["serviceTime"]),
            "#d9534f"
        ))

    for phase in data["phases"]:
        color = (
            "#1b9e8f"
            if phase["direction"] == "NORTH-SOUTH"
            else "#e0a030"
        )
        label = "%s %ds" % (
            "N-S" if phase["direction"] == "NORTH-SOUTH" else "E-W",
            phase["green"]
        )
        segments.append((
            label,
            phase["start"],
            phase["start"] + phase["green"],
            color
        ))

    if not segments:
        return "<p>No signal activity.</p>"

    total = max(end for _, _, end, _ in segments)
    if total <= 0:
        return "<p>No signal activity.</p>"

    bars = ""
    for label, start, end, color in segments:
        left = 100.0 * start / total
        width = 100.0 * (end - start) / total
        bars += (
            '<div title="%s: %d-%d s" style="position:absolute;left:%.2f%%;'
            'width:%.2f%%;top:0;bottom:0;background:%s;color:#fff;'
            'font-size:12px;line-height:44px;text-align:center;'
            'overflow:hidden;border-right:1px solid #0003;">%s</div>'
            % (label, start, end, left, width, color, label)
        )

    return (
        '<div style="position:relative;height:44px;background:#8884;'
        'border-radius:6px;overflow:hidden;">%s</div>'
        '<div style="display:flex;justify-content:space-between;'
        'font-size:12px;opacity:.7;">'
        '<span>0 s</span><span>%d s</span></div>'
        % (bars, total)
    )


def compare_lines(data):
    """Describe Adaptive vs Fixed-timer results."""
    fixed = data["comparison"]["fixed"]
    adaptive = data["comparison"]["adaptive"]
    lines = []

    def change(label, fixed_value, adaptive_value):
        if fixed_value <= 0:
            return

        percent = (fixed_value - adaptive_value) * 100.0 / fixed_value

        if abs(percent) < 0.05:
            lines.append(
                "%s: the same with both controllers (%g)"
                % (label, fixed_value)
            )
            return

        word = "lower" if percent >= 0 else "higher"
        lines.append(
            "%s: %.1f%% %s with adaptive (%g -> %g)"
            % (label, abs(percent), word, fixed_value, adaptive_value)
        )

    change("Average wait", fixed["avgWait"], adaptive["avgWait"])
    change(
        "Time to clear all vehicles",
        fixed["maxWait"],
        adaptive["maxWait"]
    )
    change(
        "Green seconds wasted",
        fixed["greenWasted"],
        adaptive["greenWasted"]
    )

    if fixed["emergencyServed"] > 0 and adaptive["emergencyServed"] > 0:
        change(
            "Emergency vehicle wait (average)",
            fixed["avgEmergencyWait"],
            adaptive["avgEmergencyWait"]
        )

        for key, label, _ in EMERGENCY_TYPES:
            fixed_kind = fixed["byType"][key]
            adaptive_kind = adaptive["byType"][key]

            if fixed_kind["served"] > 0 and adaptive_kind["served"] > 0:
                change(
                    label + " wait",
                    fixed_kind["avgWait"],
                    adaptive_kind["avgWait"]
                )

    return lines


def tradeoff_note(data):
    """Explain when emergency priority increases average traffic waiting time."""
    fixed = data["comparison"]["fixed"]
    adaptive = data["comparison"]["adaptive"]

    if (
        adaptive["emergencyServed"] > 0
        and adaptive["avgWait"] > fixed["avgWait"]
        and adaptive["avgEmergencyWait"] < fixed["avgEmergencyWait"]
    ):
        return (
            "Adaptive's average wait is higher here mainly because normal traffic "
            "waits while the green corridor clears the emergency vehicle(s) first. "
            "That is the design trade-off: emergency vehicles wait much less, "
            "everyone else waits a little more."
        )

    return ""


def emergency_breakdown(data):
    """Return the total ambulance, fire truck, and police vehicle counts."""
    parts = []

    for (key, label, icon), field in zip(
        EMERGENCY_TYPES,
        ["ambulances", "fireTrucks", "policeCars"]
    ):
        parts.append(
            "%s %d"
            % (icon, sum(lane[field] for lane in data["lanes"]))
        )

    return "  ·  ".join(parts)
