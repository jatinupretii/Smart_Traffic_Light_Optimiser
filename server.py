"""
server.py - connects the web simulation (simulation.html) to the C++ backend.

    python server.py            then open http://localhost:8000   (it also opens the browser)

How the pieces talk to each other:

    browser (simulation.html)  --HTTP-->  server.py (Python)  --runs-->  traffic_backend (C++ program)
                               <--JSON--                      <--JSON--

* The C++ program is compiled from main.cpp the first time (and again when main.cpp changes).
* For every traffic the page sends, Python starts the C++ program in --batch mode and returns the
  JSON report that the C++ program prints. The page compares it with its own numbers and shows a
  badge: "Checked with the real C++ backend".
* Only the Python standard library is used (plus traffic_core.py). Nothing needs to be installed.
* The server listens on 127.0.0.1 only, so it is reachable from this computer and no other.
"""
import argparse
import json
import os
import sys
import tempfile
import threading
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import traffic_core as tc

HERE = os.path.dirname(os.path.abspath(__file__))
WORK = os.path.join(HERE, "work")
PAGE = os.path.join(HERE, "simulation.html")      # the page that is served (see --page)
MAX_BODY = 200000          # bytes; the biggest traffic (400 vehicles) is about 3 KB

lock = threading.Lock()    # compile and run the C++ program one request at a time
config = {"cpp": "", "exe": ""}


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
        self.send_json(404, {"ok": False, "error": "not found"})

    def do_POST(self):
        if self.path.split("?", 1)[0] != "/api/run":
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
        except Exception as error:      # for example a timeout of the C++ program
            self.send_json(500, {"ok": False, "error": "%s: %s" % (type(error).__name__, error)})
        finally:
            try:
                os.remove(temp_path)
            except OSError:
                pass


def main():
    parser = argparse.ArgumentParser(description="Connects simulation.html to the C++ backend")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument("--source", default="", help="C++ source file (default: main.cpp next to this script)")
    parser.add_argument("--page", default="simulation.html", help="which page to serve, for example simulation_full.html")
    parser.add_argument("--no-browser", action="store_true", help="do not open the browser")
    args = parser.parse_args()

    global PAGE
    if os.path.basename(args.page) != args.page or not args.page.endswith(".html"):
        print("--page must be the name of an .html file in this folder, for example simulation_full.html")
        sys.exit(1)
    PAGE = os.path.join(HERE, args.page)

    config["cpp"] = args.source or tc.find_backend_source(HERE)
    config["exe"] = tc.backend_exe_path(HERE)

    ok, message = backend_status()
    print(("C++ backend ready: " if ok else "C++ backend NOT ready: ") + message)
    if config["cpp"]:
        print("C++ source:", config["cpp"])

    try:
        server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    except OSError as error:
        print("Cannot use port %d (%s). Try:  python server.py --port 8001" % (args.port, error))
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
