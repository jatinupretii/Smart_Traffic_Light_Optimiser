"""
Smart Traffic Light Optimiser - dashboard (front end).

    pip install streamlit ultralytics opencv-python
    streamlit run app.py

Flow: video/image -> YOLO detection -> lane queues (detections.txt)
      -> C++ backend (heap, queues, BFS green corridor) -> ONE result page.
"""
import json
import os
import tempfile

import pandas as pd
import streamlit as st

import traffic_core as tc

HERE = os.path.dirname(os.path.abspath(__file__))
WORK = os.path.join(HERE, "work")

st.set_page_config(page_title="Smart Traffic Light Optimiser", page_icon="🚦", layout="wide")


@st.cache_resource(show_spinner="Loading YOLO model (the first run downloads it)...")
def load_model(path):
    from ultralytics import YOLO
    return YOLO(path)


def upload_path(upload):
    """Saves the uploaded video/image once and remembers where."""
    key = (upload.name, upload.size)
    if st.session_state.get("upload_key") != key:
        folder = os.path.join(tempfile.gettempdir(), "traffic_uploads")
        os.makedirs(folder, exist_ok=True)
        path = os.path.join(folder, upload.name)
        with open(path, "wb") as f:
            f.write(upload.getvalue())
        st.session_state["upload_key"] = key
        st.session_state["upload_path"] = path
    return st.session_state["upload_path"]


# ----------------------------------------------------------------------------
# Sidebar: inputs
# ----------------------------------------------------------------------------

with st.sidebar:
    st.header("Input")
    use_yolo = st.radio("Detection source",
                        ["Video / image (YOLO)", "detections.txt (no YOLO)"]) == "Video / image (YOLO)"

    upload = None
    if use_yolo:
        upload = st.file_uploader("Traffic video or image",
                                  type=["mp4", "avi", "mov", "mkv", "jpg", "jpeg", "png"])
        model_path = st.text_input("YOLO weights", "yolov8n.pt")
        conf = st.slider("Confidence threshold", 0.10, 0.90, 0.35, 0.05)
        frames_n = st.slider("Video frames to sample", 1, 15, 5)
        step = st.slider("Use every Nth frame", 1, 60, 10)
        front = st.selectbox("Stop line is nearest the image's", ["bottom", "top", "left", "right"])
        roi_text = st.text_input("Lane regions: x1,y1,x2,y2;... (North;South;East;West). "
                                 "Blank = four quadrants", "")
        demo_emergency = st.text_input("Demo emergency vehicles (NOT detections), "
                                       "e.g. North:2:fire,East:1:police", "")
    else:
        det_file_text = st.text_input("Detections file", os.path.join(HERE, "detections.txt"))

    st.header("Comparison")
    fixed_green = st.slider("Fixed-timer green time (s)", 10, 60, 30, 5)
    fixed_lane = st.checkbox("Fixed road also has the green corridor lane (no signal priority)", value=False)
    run = st.button("Run optimiser", type="primary")


# ----------------------------------------------------------------------------
# Pipeline
# ----------------------------------------------------------------------------

def run_pipeline():
    os.makedirs(WORK, exist_ok=True)
    notes = []
    overlay = None

    if use_yolo:
        if upload is None:
            raise RuntimeError("Upload a traffic video or image first.")
        frames = tc.read_frames(upload_path(upload), frames_n, step)
        height, width = frames[0].shape[:2]
        rois = tc.parse_rois(roi_text, width, height)
        model = load_model(model_path)

        samples = []
        first_detections = []
        for index, frame in enumerate(frames):
            detections = tc.detect(model, frame, conf)
            if index == 0:
                first_detections = detections
            samples.append(tc.build_lane_lists(detections, rois, front))

        lanes = tc.pick_representative(samples)
        if demo_emergency.strip():
            notes += tc.apply_demo_emergency(lanes, demo_emergency)

        det_file = os.path.join(WORK, "detections.txt")
        tc.write_detections(det_file, lanes)
        overlay = tc.draw_overlay(frames[0], rois, first_detections)
    else:
        det_file = det_file_text
        if not os.path.exists(det_file):
            raise RuntimeError("Detections file not found: " + det_file)

    cpp = tc.find_backend_source(HERE)
    exe = tc.backend_exe_path(HERE)
    ok, message = tc.ensure_backend(cpp, exe)
    if not ok:
        raise RuntimeError(message)

    data = tc.run_backend(exe, det_file, fixed_green, bool(fixed_lane))
    return {"data": data, "overlay": overlay, "notes": notes, "detections": det_file}


# ----------------------------------------------------------------------------
# One unified result page
# ----------------------------------------------------------------------------

def show_results(res):
    data = res["data"]
    summary = data["summary"]
    emergency_total = sum(lane["emergency"] for lane in data["lanes"])

    for note in res["notes"]:
        st.warning(note)
    if summary["remaining"] > 0 or summary["routeFailures"] > 0:
        st.warning("%d vehicle(s) were not served (route failures: %d)."
                   % (summary["remaining"], summary["routeFailures"]))

    columns = st.columns(5)
    columns[0].metric("Vehicles detected", summary["totalVehicles"])
    columns[1].metric("Emergency vehicles", emergency_total)
    columns[2].metric("Average wait", "%.1f s" % summary["avgWait"])
    columns[3].metric("Time to clear all", "%d s" % summary["maxWait"])
    columns[4].metric("Signal cycles", summary["cycles"])

    if emergency_total:
        st.caption("Emergency vehicles:  " + tc.emergency_breakdown(data))

    if data["phases"]:
        first = data["phases"][0]
        st.success("Next green: %s for %d s%s" % (
            first["direction"], first["green"],
            "  (after clearing %d emergency vehicle(s) through the green corridor)" % emergency_total
            if emergency_total else ""))

    # ---- detection ---------------------------------------------------------
    left, right = st.columns([3, 2])
    with left:
        st.markdown("#### Detection")
        if res["overlay"] is not None:
            st.image(res["overlay"][:, :, ::-1],
                     caption="First sampled frame: lane regions and YOLO detections")
        else:
            st.info("Detections were read from a file, so there is no video frame to show.")
    with right:
        st.markdown("#### Vehicles per lane")
        lanes_df = pd.DataFrame(data["lanes"])
        st.bar_chart(pd.DataFrame({"Normal": lanes_df["normal"].values,
                                   "Ambulance": lanes_df["ambulances"].values,
                                   "Fire truck": lanes_df["fireTrucks"].values,
                                   "Police car": lanes_df["policeCars"].values},
                                  index=lanes_df["name"]))
        st.dataframe(lanes_df.rename(columns={"name": "Lane", "detected": "Detected",
                                              "normal": "Normal queue", "ambulances": "Ambulance",
                                              "fireTrucks": "Fire truck", "policeCars": "Police car"})
                     [["Lane", "Detected", "Normal queue", "Ambulance", "Fire truck", "Police car"]],
                     hide_index=True)

    # ---- signal plan -------------------------------------------------------
    st.markdown("#### Signal plan (adaptive controller)")
    if data["phases"]:
        st.markdown(tc.timeline_html(data), unsafe_allow_html=True)
        st.caption("Red = ambulance green corridor, teal = North-South green, amber = East-West "
                   "green. Gaps are the yellow + all-red changeover.")
        rows = [{"Cycle": p["cycle"], "Direction": p["direction"], "Green (s)": p["green"],
                 "Starts at (s)": p["start"],
                 "Vehicles released": ", ".join("%s %d" % (lane["name"], lane["served"])
                                                for lane in p["lanes"])}
                for p in data["phases"]]
        st.dataframe(pd.DataFrame(rows), hide_index=True)
    else:
        st.info("No normal vehicles to serve.")

    # ---- emergency corridor ------------------------------------------------
    st.markdown("#### Emergency green corridor lane")
    if data["emergencies"]:
        icons = {label: icon for _, label, icon in
                 [("", "Ambulance", "🚑"), ("", "Fire Truck", "🚒"), ("", "Police Car", "🚓")]}
        for order, e in enumerate(data["emergencies"], 1):
            st.markdown("%s **%d. %s #%d**  ·  %s lane  ·  priority %d  \n"
                        "&nbsp;&nbsp;&nbsp;&nbsp;%s  (route %d units · crosses the junction at t = %d s)"
                        % (icons.get(e["type"], "🚨"), order, e["type"], e["id"], e["lane"],
                           e["priority"], " → ".join(e["route"]), e["distance"], e["serviceTime"]))
        st.caption("Dispatch order comes from the priority heap: ambulance 100, fire truck 90, "
                   "police car 70 (ties: smaller vehicle ID). Each vehicle waits in the dedicated green "
                   "corridor lane, crosses the junction in 3 s while all signals are red (5 s in total); "
                   "the rest of its route is a green hold at the next junctions (not simulated). "
                   "On the fixed road an emergency vehicle waits inside the normal queue. "
                   "Destinations are demo assumptions.")
    else:
        st.info("No emergency vehicles detected.")

    # ---- comparison --------------------------------------------------------
    st.markdown("#### Fixed-timer vs Adaptive (same traffic)")
    fixed = data["comparison"]["fixed"]
    adaptive = data["comparison"]["adaptive"]
    fixed_label = "Fixed-timer (%d s)" % data["comparison"]["fixedGreen"]

    rows = [("Vehicles served", str(fixed["served"]), str(adaptive["served"])),
            ("Average wait (s)", "%.1f" % fixed["avgWait"], "%.1f" % adaptive["avgWait"]),
            ("Time to clear all (s)", str(fixed["maxWait"]), str(adaptive["maxWait"]))]
    if fixed["emergencyServed"] > 0:
        rows.append(("Emergency wait, average (s)", "%.1f" % fixed["avgEmergencyWait"],
                     "%.1f" % adaptive["avgEmergencyWait"]))
        for key, label, icon in tc.EMERGENCY_TYPES:
            fixed_kind = fixed["byType"][key]
            adaptive_kind = adaptive["byType"][key]
            if fixed_kind["served"] > 0 or adaptive_kind["served"] > 0:
                rows.append(("    %s %s wait (s)" % (icon, label), "%.1f" % fixed_kind["avgWait"],
                             "%.1f" % adaptive_kind["avgWait"]))
    rows += [("Green phases used", str(fixed["phases"]), str(adaptive["phases"])),
             ("Green seconds wasted", str(fixed["greenWasted"]), str(adaptive["greenWasted"]))]

    table = pd.DataFrame({"Metric": [r[0] for r in rows],
                          fixed_label: [r[1] for r in rows],
                          "Adaptive": [r[2] for r in rows]})
    st.dataframe(table, hide_index=True)

    chart_left, chart_right = st.columns(2)
    with chart_left:
        st.caption("Average wait (s)")
        st.bar_chart(pd.DataFrame({"Fixed": [fixed["avgWait"]], "Adaptive": [adaptive["avgWait"]]},
                                  index=["Average wait"]))
    with chart_right:
        st.caption("Time to clear all vehicles (s)")
        st.bar_chart(pd.DataFrame({"Fixed": [fixed["maxWait"]], "Adaptive": [adaptive["maxWait"]]},
                                  index=["Clear time"]))

    for line in tc.compare_lines(data):
        st.markdown("- " + line)
    note = tc.tradeoff_note(data)
    if note:
        st.info(note)
    st.caption("Model: every detected vehicle is waiting at t = 0 and no new vehicles arrive, so "
               "this is a snapshot comparison, not a long-run traffic simulation.")

    # ---- animated simulation (separate web page) ---------------------------
    st.markdown("#### Animated simulation")
    st.info("To watch this traffic as an animation, open **simulation.html** in your browser, "
            "press **Load detections.txt** and choose:  `%s`" % res.get("detections", "detections.txt"))

    # ---- details -----------------------------------------------------------
    with st.expander("Every vehicle served (history)"):
        st.dataframe(pd.DataFrame(data["history"]), hide_index=True)
    with st.expander("Raw backend report (JSON)"):
        st.json(data)
    st.download_button("Download report (JSON)", json.dumps(data, indent=2),
                       file_name="traffic_report.json", mime="application/json")


# ----------------------------------------------------------------------------
# Page
# ----------------------------------------------------------------------------

st.title("🚦 Smart Traffic Light Optimiser")
st.caption("YOLO detects vehicles  →  C++ backend (queues, priority heap, BFS green corridor) "
           "decides the signal plan  →  compared against a fixed timer")

if use_yolo and upload is not None:
    try:
        preview_frames = tc.read_frames(upload_path(upload), 1, 1)
        preview_h, preview_w = preview_frames[0].shape[:2]
        preview_rois = tc.parse_rois(roi_text, preview_w, preview_h)
        with st.expander("Lane regions preview (check these before running)",
                         expanded="result" not in st.session_state):
            st.image(tc.draw_overlay(preview_frames[0], preview_rois, [], show_counts=False)[:, :, ::-1],
                     caption="Every vehicle inside a region counts for that lane. "
                             "Edit the regions in the sidebar.")
    except Exception as error:
        st.error("Could not read the upload or the lane regions: %s" % error)

if run:
    try:
        with st.spinner("Detecting vehicles and running the optimiser..."):
            st.session_state["result"] = run_pipeline()
    except Exception as error:
        st.session_state.pop("result", None)
        st.error(str(error))

if "result" in st.session_state:
    show_results(st.session_state["result"])
else:
    st.info("Choose a source in the sidebar and press **Run optimiser**. "
            "No video yet? Pick 'detections.txt (no YOLO)' to try the backend with the sample file.")
