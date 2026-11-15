"""
traffic_core.py - everything the dashboard needs, with no Streamlit code inside.

  vision part  : lane regions, YOLO detection, queue ordering, detections.txt
  backend part : compile and run the C++ program in --batch mode (returns JSON)
  report part  : timeline and comparison text for the dashboard
"""
import json
import os
import shutil
import subprocess

LANES = ["North", "South", "East", "West"]

# YOLO class name (lower case; "_" and "-" are read as spaces) -> token in detections.txt.
# The standard COCO model only knows car, bus, motorcycle and truck. The emergency
# classes need a custom-trained model whose class names match one of these.
CLASS_MAP = {
    "car": "car",
    "bus": "bus",
    "motorcycle": "motorbike",
    "truck": "truck",
    "ambulance": "ambulance",
    "fire truck": "firetruck",
    "firetruck": "firetruck",
    "fire engine": "firetruck",
    "police car": "police",
    "police": "police",
    "policecar": "police",
    "police vehicle": "police",
}

EMERGENCY_TOKENS = ("ambulance", "firetruck", "police")

# (key in the backend JSON, label, icon) - same order as the backend (priority 100 / 90 / 70)
EMERGENCY_TYPES = [("ambulance", "Ambulance", "🚑"),
                   ("fireTruck", "Fire truck", "🚒"),
                   ("policeCar", "Police car", "🚓")]

DEMO_KINDS = {"ambulance": "ambulance", "amb": "ambulance", "fire": "firetruck",
              "firetruck": "firetruck", "police": "police"}


def class_token(name):
    """YOLO class name -> detections.txt token, or None if it is not a vehicle we use."""
    return CLASS_MAP.get(name.lower().replace("_", " ").replace("-", " "))

# BGR colours for drawing (North, South, East, West)
LANE_COLORS = [(255, 128, 0), (0, 200, 0), (0, 165, 255), (200, 0, 200)]


# ----------------------------------------------------------------------------
# Vision: lanes, ordering, detections file
# ----------------------------------------------------------------------------

def parse_rois(text, width, height):
    """Four lane rectangles (x1,y1,x2,y2) in the order North;South;East;West."""
    if not text.strip():
        w2, h2 = width // 2, height // 2
        return [(0, 0, w2, h2), (0, h2, w2, height),
                (w2, 0, width, h2), (w2, h2, width, height)]
    parts = text.strip().split(";")
    if len(parts) != 4:
        raise ValueError("Lane regions need exactly 4 rectangles: North;South;East;West")
    rois = []
    for part in parts:
        x1, y1, x2, y2 = [int(v) for v in part.split(",")]
        rois.append((x1, y1, x2, y2))
    return rois


def lane_of(cx, cy, rois):
    for index, (x1, y1, x2, y2) in enumerate(rois):
        if x1 <= cx < x2 and y1 <= cy < y2:
            return index
    return None


def front_key(box, front):
    """Smaller key = closer to the stop line."""
    x1, y1, x2, y2 = box
    if front == "bottom":
        return -y2
    if front == "top":
        return y1
    if front == "left":
        return x1
    return -x2


def build_lane_lists(detections, rois, front):
    """detections: list of (class_name, (x1, y1, x2, y2)).
    Returns 4 lists of vehicle names, front of queue first."""
    lanes = [[] for _ in range(4)]
    for name, box in detections:
        token = class_token(name)
        if token is None:
            continue
        cx = (box[0] + box[2]) / 2
        cy = (box[1] + box[3]) / 2
        lane = lane_of(cx, cy, rois)
        if lane is not None:
            lanes[lane].append((front_key(box, front), token))
    return [[name for _, name in sorted(items)] for items in lanes]


def pick_representative(samples):
    """Per lane, keep the sampled frame whose vehicle count is the median,
    so one bad frame (missed or double-counted vehicles) does not decide."""
    result = []
    for lane in range(4):
        order = sorted(range(len(samples)), key=lambda i: len(samples[i][lane]))
        result.append(samples[order[len(order) // 2]][lane])
    return result


def apply_demo_emergency(lanes, spec):
    """spec like "North:2:fire,East:1:police,West:3" (kind = ambulance | fire | police,
    default ambulance). Inserts that emergency vehicle at the queue position.
    Returns warning messages. This is NOT a detection."""
    messages = []
    for item in spec.split(","):
        item = item.strip()
        if not item:
            continue
        parts = [part.strip() for part in item.split(":")]
        if len(parts) not in (2, 3):
            raise ValueError("Demo emergency format is Lane:position[:ambulance|fire|police], "
                             "for example North:2:fire")
        names = [n.lower() for n in LANES]
        if parts[0].lower() not in names:
            raise ValueError("Unknown lane '%s' (use North, South, East or West)" % parts[0])
        lane = names.index(parts[0].lower())
        kind = "ambulance"
        if len(parts) == 3:
            if parts[2].lower() not in DEMO_KINDS:
                raise ValueError("Unknown vehicle kind '%s' (use ambulance, fire or police)" % parts[2])
            kind = DEMO_KINDS[parts[2].lower()]
        index = max(0, min(int(parts[1]) - 1, len(lanes[lane])))
        lanes[lane].insert(index, kind)
        messages.append("DEMO OVERRIDE: %s inserted in %s at position %d "
                        "(manually added, not detected by YOLO)" % (kind, LANES[lane], index + 1))
    return messages


apply_demo_ambulances = apply_demo_emergency      # old name, kept so nothing breaks


def write_detections(path, lanes):
    with open(path, "w") as f:
        f.write("# lane: vehicles from the stop line (front of queue) to the back\n")
        for name, sequence in zip(LANES, lanes):
            f.write(name + ": " + " ".join(sequence) + "\n")


def read_detections(path):
    """Reads a detections file back into 4 lists (used when YOLO is skipped)."""
    lanes = [[] for _ in range(4)]
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or ":" not in line:
                continue
            name, rest = line.split(":", 1)
            names = [n.lower() for n in LANES]
            if name.strip().lower() in names:
                lanes[names.index(name.strip().lower())] = rest.split()
    return lanes


def read_frames(source, count, step):
    import cv2
    image = cv2.imread(source)
    if image is not None:
        return [image]
    capture = cv2.VideoCapture(source)
    if not capture.isOpened():
        raise RuntimeError("Cannot open the video/image: " + source)
    frames = []
    index = 0
    while len(frames) < count:
        ok, frame = capture.read()
        if not ok:
            break
        if index % step == 0:
            frames.append(frame)
        index += 1
    capture.release()
    if not frames:
        raise RuntimeError("No frames could be read from " + source)
    return frames


def detect(model, frame, conf):
    """Runs YOLO on one frame -> list of (class_name, (x1, y1, x2, y2))."""
    result = model(frame, conf=conf, verbose=False)[0]
    boxes = result.boxes.xyxy.tolist()
    classes = result.boxes.cls.tolist()
    return [(model.names[int(c)], tuple(b)) for b, c in zip(boxes, classes)]


def draw_overlay(frame, rois, detections, show_counts=True):
    """Lane rectangles (with counts when show_counts), and a box around every detected vehicle."""
    import cv2
    shown = frame.copy()
    counts = [0, 0, 0, 0]

    for name, box in detections:
        token = class_token(name)
        if token is None:
            continue
        x1, y1, x2, y2 = [int(v) for v in box]
        lane = lane_of((x1 + x2) / 2, (y1 + y2) / 2, rois)
        if lane is None:
            continue
        counts[lane] += 1
        emergency = token in EMERGENCY_TOKENS
        color = (0, 0, 255) if emergency else LANE_COLORS[lane]
        cv2.rectangle(shown, (x1, y1), (x2, y2), color, 3 if emergency else 2)
        cv2.putText(shown, token.upper() if emergency else token, (x1, max(15, y1 - 5)),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6 if emergency else 0.5, color,
                    2 if emergency else 1)

    for lane, (x1, y1, x2, y2) in enumerate(rois):
        cv2.rectangle(shown, (x1, y1), (x2 - 1, y2 - 1), LANE_COLORS[lane], 3)
        label = "%s: %d" % (LANES[lane], counts[lane]) if show_counts else LANES[lane]
        cv2.putText(shown, label, (x1 + 8, y1 + 28),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.9, LANE_COLORS[lane], 2)
    return shown


# ----------------------------------------------------------------------------
# C++ backend
# ----------------------------------------------------------------------------

def find_backend_source(folder):
    for name in ["main.cpp", "smart_traffic_optimiser.cpp"]:
        path = os.path.join(folder, name)
        if os.path.exists(path):
            return path
    return ""


def backend_exe_path(folder):
    return os.path.join(folder, "traffic_backend.exe" if os.name == "nt" else "traffic_backend")


def ensure_backend(cpp, exe):
    """Compiles the C++ backend when it is missing or older than the source.
    Returns (ok, message)."""
    if not cpp or not os.path.exists(cpp):
        return False, "C++ source not found. Put main.cpp in the same folder as the Python files."
    if os.path.exists(exe) and os.path.getmtime(exe) >= os.path.getmtime(cpp):
        return True, "Backend is up to date."
    compiler = shutil.which("g++")
    if not compiler:
        return False, ("g++ was not found. Install MinGW-w64, or compile by hand:\n"
                       "g++ main.cpp -o " + os.path.basename(exe))
    result = subprocess.run([compiler, "-std=c++11", "-O2", cpp, "-o", exe],
                            capture_output=True, text=True)
    if result.returncode != 0:
        return False, "C++ compile failed:\n" + result.stderr[-1500:]
    return True, "Backend compiled."


def run_backend(exe, detections_path, fixed_green, fixed_lane=False):
    """Runs the backend in --batch mode and returns its JSON report as a dict.
    fixed_lane=True gives the fixed road the green corridor lane too (optional comparison)."""
    result = subprocess.run([exe, "--batch", detections_path, str(int(fixed_green)), "1" if fixed_lane else "0"],
                            capture_output=True, text=True, timeout=60)
    try:
        data = json.loads(result.stdout)
    except ValueError:
        raise RuntimeError("Backend did not return a JSON report.\n"
                           + result.stdout[-500:] + result.stderr[-500:])
    if not data.get("ok"):
        raise RuntimeError(data.get("error", "Backend reported an error"))
    return data


# ----------------------------------------------------------------------------
# Report helpers for the dashboard
# ----------------------------------------------------------------------------

def timeline_html(data):
    """Signal timeline: red = junction held for an emergency vehicle, teal = North-South green,
    amber = East-West green. Gaps are yellow/all-red changeover time."""
    segments = []
    for e in data["emergencies"]:
        # junction held red for this vehicle: from "start" until "end" (older reports only had serviceTime)
        segments.append(("Corridor", e.get("start", 0), e.get("end", e["serviceTime"]), "#d9534f"))
    for p in data["phases"]:
        color = "#1b9e8f" if p["direction"] == "NORTH-SOUTH" else "#e0a030"
        label = "%s %ds" % ("N-S" if p["direction"] == "NORTH-SOUTH" else "E-W", p["green"])
        segments.append((label, p["start"], p["start"] + p["green"], color))

    if not segments:
        return "<p>No signal activity.</p>"

    total = max(end for _, _, end, _ in segments)
    bars = ""
    for label, start, end, color in segments:
        left = 100.0 * start / total
        width = 100.0 * (end - start) / total
        bars += ('<div title="%s: %d-%d s" style="position:absolute;left:%.2f%%;width:%.2f%%;'
                 'top:0;bottom:0;background:%s;color:#fff;font-size:12px;line-height:44px;'
                 'text-align:center;overflow:hidden;border-right:1px solid #0003;">%s</div>'
                 % (label, start, end, left, width, color, label))
    return ('<div style="position:relative;height:44px;background:#8884;border-radius:6px;'
            'overflow:hidden;">%s</div>'
            '<div style="display:flex;justify-content:space-between;font-size:12px;opacity:.7;">'
            '<span>0 s</span><span>%d s</span></div>' % (bars, total))


def compare_lines(data):
    """Plain sentences describing Adaptive vs Fixed-timer, including bad news."""
    fixed = data["comparison"]["fixed"]
    adaptive = data["comparison"]["adaptive"]
    lines = []

    def change(label, fixed_value, adaptive_value):
        if fixed_value <= 0:
            return
        percent = (fixed_value - adaptive_value) * 100.0 / fixed_value
        if abs(percent) < 0.05:
            lines.append("%s: the same with both controllers (%g)" % (label, fixed_value))
            return
        word = "lower" if percent >= 0 else "higher"
        lines.append("%s: %.1f%% %s with adaptive (%g -> %g)"
                     % (label, abs(percent), word, fixed_value, adaptive_value))

    change("Average wait", fixed["avgWait"], adaptive["avgWait"])
    change("Time to clear all vehicles", fixed["maxWait"], adaptive["maxWait"])
    change("Green seconds wasted", fixed["greenWasted"], adaptive["greenWasted"])
    if fixed["emergencyServed"] > 0 and adaptive["emergencyServed"] > 0:
        change("Emergency vehicle wait (average)", fixed["avgEmergencyWait"],
               adaptive["avgEmergencyWait"])
        for key, label, _ in EMERGENCY_TYPES:
            fixed_kind = fixed["byType"][key]
            adaptive_kind = adaptive["byType"][key]
            if fixed_kind["served"] > 0 and adaptive_kind["served"] > 0:
                change(label + " wait", fixed_kind["avgWait"], adaptive_kind["avgWait"])
    return lines


def tradeoff_note(data):
    """Honest explanation when emergency priority makes the average wait worse."""
    fixed = data["comparison"]["fixed"]
    adaptive = data["comparison"]["adaptive"]
    if (adaptive["emergencyServed"] > 0 and adaptive["avgWait"] > fixed["avgWait"]
            and adaptive["avgEmergencyWait"] < fixed["avgEmergencyWait"]):
        return ("Adaptive's average wait is higher here mainly because normal traffic waits "
                "while the green corridor clears the emergency vehicle(s) first. That is the design "
                "trade-off: emergency vehicles wait much less, everyone else waits a little more.")
    return ""


def emergency_breakdown(data):
    """e.g. '🚑 1  ·  🚒 2  ·  🚓 0' summed over all lanes."""
    parts = []
    for (key, label, icon), field in zip(EMERGENCY_TYPES, ["ambulances", "fireTrucks", "policeCars"]):
        parts.append("%s %d" % (icon, sum(lane[field] for lane in data["lanes"])))
    return "  ·  ".join(parts)
