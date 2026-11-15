"""
server.py - serves the original simulation and connects it to the C++ backend.
Run from the project virtual environment:
    python server.py
Then open http://localhost:8000
"""
import argparse
import base64
import binascii
import json
import os
import re
import sys
import tempfile
import threading
import uuid
import webbrowser
import subprocess
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

import traffic_core as tc

HERE = os.path.dirname(os.path.abspath(__file__))
WORK = os.path.join(HERE, "work")
PAGE = os.path.join(HERE, "simulation.html")
DETECTOR = os.path.join(HERE, "lane_mapper_auto.py")
YOLO_MODEL = os.path.join(HERE, "yolo11n.pt")
MAX_BODY = 18 * 1024 * 1024
MAX_IMAGE_BYTES = 12 * 1024 * 1024

lock = threading.Lock()
config = {"cpp": "", "exe": ""}
latest_detection_image = os.path.join(WORK, "lane_visualization.jpg")


def backend_status():
    with lock:
        return tc.ensure_backend(config["cpp"], config["exe"])


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        sys.stderr.write("[server] " + (fmt % args) + "\n")

    def send_json(self, code, obj):
        body = json.dumps(obj).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def send_file(self, path, content_type):
        try:
            with open(path, "rb") as file:
                body = file.read()
        except OSError:
            self.send_json(404, {"ok": False, "error": "Requested file was not found"})
            return
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        path = self.path.split("?", 1)[0]
        if path in ("/", "/simulation.html"):
            try:
                with open(PAGE, "rb") as f:
                    body = f.read()
            except OSError:
                self.send_json(404, {"ok": False, "error": "simulation.html was not found next to server.py"})
                return
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)
            return
        if path == "/api/status":
            ok, message = backend_status()
            self.send_json(200, {"ok": ok, "message": message})
            return
        if path == "/api/detection-image":
            self.send_file(latest_detection_image, "image/jpeg")
            return
        self.send_json(404, {"ok": False, "error": "not found"})

    def do_POST(self):
        path = self.path.split("?", 1)[0]
        if path not in ("/api/run", "/api/detect"):
            self.send_json(404, {"ok": False, "error": "not found"})
            return

        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            length = -1
        if length < 0:
            self.send_json(400, {"ok": False, "error": "bad Content-Length"})
            return
        if length > MAX_BODY:
            self.send_json(413, {"ok": False, "error": "request too large"})
            return
        try:
            payload = json.loads(self.rfile.read(length).decode("utf-8"))
        except (ValueError, UnicodeDecodeError):
            self.send_json(400, {"ok": False, "error": "body must be JSON"})
            return

        if path == "/api/run":
            self.handle_run(payload)
        else:
            self.handle_detect(payload)

    def handle_run(self, payload):
        if not isinstance(payload, dict) or not isinstance(payload.get("text"), str):
            self.send_json(400, {"ok": False, "error": 'JSON needs a "text" string (the traffic, as in detections.txt)'})
            return
        try:
            fixed_green = int(payload.get("fixedGreen", 30))
        except (TypeError, ValueError):
            self.send_json(400, {"ok": False, "error": "fixedGreen must be a number"})
            return
        fixed_green = max(10, min(60, fixed_green))
        fixed_lane = payload.get("fixedLane") is True

        os.makedirs(WORK, exist_ok=True)
        handle, temp_path = tempfile.mkstemp(suffix=".txt", dir=WORK)
        try:
            with os.fdopen(handle, "w", encoding="utf-8") as f:
                f.write(payload["text"])
            with lock:
                ok, message = tc.ensure_backend(config["cpp"], config["exe"])
                if not ok:
                    raise RuntimeError(message)
                data = tc.run_backend(config["exe"], temp_path, fixed_green, fixed_lane)
            self.send_json(200, data)
        except RuntimeError as error:
            self.send_json(500, {"ok": False, "error": str(error)})
        except Exception as error:
            self.send_json(500, {"ok": False, "error": "%s: %s" % (type(error).__name__, error)})
        finally:
            try:
                os.remove(temp_path)
            except OSError:
                pass

    def handle_detect(self, payload):
        global latest_detection_image
        if not isinstance(payload, dict):
            self.send_json(400, {"ok": False, "error": "JSON body must be an object"})
            return
        encoded = payload.get("image")
        filename = payload.get("filename", "traffic-image.jpg")
        if not isinstance(encoded, str):
            self.send_json(400, {"ok": False, "error": 'JSON needs an "image" base64 string'})
            return
        ext = os.path.splitext(str(filename))[1].lower()
        if ext not in (".jpg", ".jpeg", ".png", ".webp"):
            self.send_json(400, {"ok": False, "error": "Use a JPG, PNG or WEBP image"})
            return
        try:
            image_bytes = base64.b64decode(encoded, validate=True)
        except (binascii.Error, ValueError):
            self.send_json(400, {"ok": False, "error": "Invalid base64 image data"})
            return
        if not image_bytes or len(image_bytes) > MAX_IMAGE_BYTES:
            self.send_json(413, {"ok": False, "error": "Image must be between 1 byte and 12 MB"})
            return
        if not os.path.isfile(DETECTOR):
            self.send_json(500, {"ok": False, "error": "lane_mapper_auto.py was not found next to server.py"})
            return
        if not os.path.isfile(YOLO_MODEL):
            self.send_json(500, {"ok": False, "error": "yolo11n.pt was not found next to server.py"})
            return

        os.makedirs(WORK, exist_ok=True)
        run_id = uuid.uuid4().hex
        image_path = os.path.join(WORK, "upload_" + run_id + ext)
        try:
            with open(image_path, "wb") as f:
                f.write(image_bytes)

            # The detector writes fixed work/ output names, so serialize the full detection + C++ run.
            with lock:
                process = subprocess.run(
                    [sys.executable, DETECTOR, image_path, "--model", YOLO_MODEL],
                    cwd=HERE, capture_output=True, text=True, timeout=180
                )
                if process.returncode != 0:
                    detail = (process.stderr or process.stdout)[-2500:]
                    raise RuntimeError("Vehicle detection failed:\n" + detail)

                detections_path = os.path.join(WORK, "lane_detections.txt")
                details_path = os.path.join(WORK, "lane_vehicle_detections.json")
                if not os.path.isfile(detections_path) or not os.path.isfile(details_path):
                    raise RuntimeError("Detector did not generate its expected output files.")

                with open(details_path, "r", encoding="utf-8") as f:
                    detection_data = json.load(f)

                ok, message = tc.ensure_backend(config["cpp"], config["exe"])
                if not ok:
                    raise RuntimeError(message)
                cpp_report = tc.run_backend(config["exe"], detections_path, 30, False)
                latest_detection_image = os.path.join(WORK, "lane_visualization.jpg")

            lane_tokens = tc.read_detections(detections_path)
            lane_counts = {
                lane: len(tokens)
                for lane, tokens in zip(tc.LANES, lane_tokens)
            }
            self.send_json(200, {
                "ok": True,
                "run_id": run_id,
                "total_detected": detection_data.get("total_detected", 0),
                "lane_counts": lane_counts,
                "unassigned_count": detection_data.get("unassigned_count", 0),
                "lane_tokens": lane_tokens,
                "cpp_report": cpp_report,
                "detector_output": process.stdout[-3000:],
                "annotated_image": "/api/detection-image"
            })
        except subprocess.TimeoutExpired:
            self.send_json(500, {"ok": False, "error": "Detection timed out after 180 seconds"})
        except Exception as error:
            self.send_json(500, {"ok": False, "error": "%s: %s" % (type(error).__name__, error)})
        finally:
            try:
                os.remove(image_path)
            except OSError:
                pass


def main():
    parser = argparse.ArgumentParser(description="Connects the simulation, YOLO detector and C++ backend")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument("--source", default="", help="C++ source file (default: main.cpp next to this script)")
    parser.add_argument("--page", default="simulation.html", help="which page to serve")
    parser.add_argument("--no-browser", action="store_true", help="do not open the browser")
    args = parser.parse_args()

    global PAGE
    if os.path.basename(args.page) != args.page or not args.page.endswith(".html"):
        print("--page must be the name of an .html file in this folder")
        sys.exit(1)
    PAGE = os.path.join(HERE, args.page)

    config["cpp"] = args.source or tc.find_backend_source(HERE)
    config["exe"] = tc.backend_exe_path(HERE)

    ok, message = backend_status()
    print(("C++ backend ready: " if ok else "C++ backend NOT ready: ") + message)
    if config["cpp"]:
        print("C++ source:", config["cpp"])
    if not os.path.isfile(DETECTOR):
        print("WARNING: lane_mapper_auto.py not found; image detection will be unavailable.")
    if not os.path.isfile(YOLO_MODEL):
        print("WARNING: yolo11n.pt not found; image detection will be unavailable.")

    try:
        server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    except OSError as error:
        print("Cannot use port %d (%s). Try: python server.py --port 8001" % (args.port, error))
        sys.exit(1)

    url = "http://localhost:%d/" % args.port
    print("Open %s   (Ctrl+C to stop)" % url)
    if not args.no_browser:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopped.")


if __name__ == "__main__":
    main()
