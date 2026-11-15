#!/usr/bin/env python3
"""
lane_mapper_auto.py
Image-only vehicle detection with overlapping tiles for small/distant vehicles,
followed by approximate four-way lane assignment.

Run:
    python lane_mapper_auto.py path/to/intersection.png --model yolo11n.pt

Outputs (used by server.py):
    work/lane_detections.txt
    work/lane_vehicle_detections.json
    work/lane_visualization.jpg

NOTE:
- This improves detection of small vehicles by running YOLO on overlapping crops.
- It does not guarantee every vehicle will be detected.
- Lane assignment is still approximate and assumes a roughly centred,
  axis-aligned four-way junction.
"""

import argparse
import json
from pathlib import Path

import cv2
from ultralytics import YOLO

LANES = ["North", "South", "East", "West"]
VEHICLE_CLASSES = {2: "car", 3: "motorbike", 5: "bus", 7: "truck"}

COLORS = {
    "North": (0, 200, 255),
    "South": (255, 150, 0),
    "East": (0, 220, 0),
    "West": (220, 0, 220),
    "Unassigned": (150, 150, 150),
}

# Detection tuning
CONFIDENCE = 0.12
IMAGE_SIZE = 960
TILE_SIZE = 768
OVERLAP = 0.25
NMS_THRESHOLD = 0.50


def tile_starts(length, tile_size, overlap):
    """Return crop starts that cover the entire dimension, including its end."""
    if length <= tile_size:
        return [0]
    step = max(1, int(tile_size * (1.0 - overlap)))
    starts = list(range(0, max(1, length - tile_size + 1), step))
    last = length - tile_size
    if starts[-1] != last:
        starts.append(last)
    return starts


def make_tiles(image):
    """Yield (crop, x_offset, y_offset) for overlapping image tiles."""
    height, width = image.shape[:2]
    xs = tile_starts(width, TILE_SIZE, OVERLAP)
    ys = tile_starts(height, TILE_SIZE, OVERLAP)

    for y in ys:
        for x in xs:
            crop = image[y:min(y + TILE_SIZE, height),
                         x:min(x + TILE_SIZE, width)]
            if crop.size:
                yield crop, x, y


def detect_on_crop(model, crop, x_offset, y_offset, confidence):
    """Detect vehicles on one crop and translate boxes to original-image coords."""
    result = model.predict(
        source=crop,
        conf=confidence,
        imgsz=IMAGE_SIZE,
        max_det=1000,
        verbose=False
    )[0]

    found = []
    if result.boxes is None:
        return found

    for box in result.boxes:
        class_id = int(box.cls[0].item())
        if class_id not in VEHICLE_CLASSES:
            continue

        x1, y1, x2, y2 = [float(v) for v in box.xyxy[0].tolist()]
        score = float(box.conf[0].item())

        found.append({
            "class_id": class_id,
            "vehicle": VEHICLE_CLASSES[class_id],
            "confidence": score,
            "bbox": [
                x1 + x_offset, y1 + y_offset,
                x2 + x_offset, y2 + y_offset
            ]
        })
    return found


def class_aware_nms(detections, image_width, image_height):
    """
    Remove duplicate boxes produced by overlapping image tiles.

    Uses normal IoU suppression plus a containment check. The containment
    check catches cases where one tile detects most of a vehicle and another
    tile detects a smaller box around the same vehicle.
    """
    candidates = []

    for detection in detections:
        x1, y1, x2, y2 = detection["bbox"]

        x1 = max(0.0, min(float(x1), image_width - 1.0))
        y1 = max(0.0, min(float(y1), image_height - 1.0))
        x2 = max(x1 + 1.0, min(float(x2), float(image_width)))
        y2 = max(y1 + 1.0, min(float(y2), float(image_height)))

        item = dict(detection)
        item["bbox"] = [x1, y1, x2, y2]
        candidates.append(item)

    # Process the most confident detections first.
    candidates.sort(key=lambda item: item["confidence"], reverse=True)
    kept = []

    def overlap_values(box_a, box_b):
        ax1, ay1, ax2, ay2 = box_a
        bx1, by1, bx2, by2 = box_b

        ix1 = max(ax1, bx1)
        iy1 = max(ay1, by1)
        ix2 = min(ax2, bx2)
        iy2 = min(ay2, by2)

        intersection = max(0.0, ix2 - ix1) * max(0.0, iy2 - iy1)
        area_a = max(1.0, (ax2 - ax1) * (ay2 - ay1))
        area_b = max(1.0, (bx2 - bx1) * (by2 - by1))
        union = area_a + area_b - intersection

        iou = intersection / max(union, 1.0)
        containment = intersection / max(min(area_a, area_b), 1.0)

        acx = (ax1 + ax2) / 2.0
        acy = (ay1 + ay2) / 2.0
        bcx = (bx1 + bx2) / 2.0
        bcy = (by1 + by2) / 2.0
        center_distance = ((acx - bcx) ** 2 + (acy - bcy) ** 2) ** 0.5

        smaller_diagonal = min(
            ((ax2 - ax1) ** 2 + (ay2 - ay1) ** 2) ** 0.5,
            ((bx2 - bx1) ** 2 + (by2 - by1) ** 2) ** 0.5
        )

        return iou, containment, center_distance, smaller_diagonal

    for candidate in candidates:
        duplicate = False

        for accepted in kept:
            iou, containment, center_distance, smaller_diagonal = overlap_values(
                candidate["bbox"], accepted["bbox"]
            )

            # Standard NMS for same-class detections.
            same_class_duplicate = (
                candidate["class_id"] == accepted["class_id"]
                and iou >= NMS_THRESHOLD
            )

            # A tile may return a partial/smaller box for the same vehicle.
            # Require strong containment and nearby box centres to avoid
            # suppressing genuinely adjacent vehicles.
            contained_duplicate = (
                candidate["class_id"] == accepted["class_id"]
                and containment >= 0.65
                and center_distance <= max(18.0, 0.40 * smaller_diagonal)
            )

            if same_class_duplicate or contained_duplicate:
                duplicate = True
                break

        if not duplicate:
            kept.append(candidate)

    return kept


def identify_lane(cx, cy, width, height, center_x, center_y, dead_zone=0.10):
    """Approximate lane based on the vehicle's road-contact point."""
    dx = (cx - center_x) / max(width, 1)
    dy = (cy - center_y) / max(height, 1)

    if abs(dx) < dead_zone and abs(dy) < dead_zone:
        return "Unassigned"

    if abs(dx) > abs(dy):
        return "East" if dx > 0 else "West"
    return "South" if dy > 0 else "North"


def main():
    parser = argparse.ArgumentParser(
        description="Detect vehicles from one image using overlapping YOLO tiles."
    )
    parser.add_argument("image", nargs="?", default="junction.png")
    parser.add_argument("--model", default="yolo11n.pt")
    parser.add_argument("--confidence", type=float, default=CONFIDENCE)
    parser.add_argument(
        "--center", nargs=2, type=float, metavar=("X", "Y"),
        help="Optional junction centre in pixels; defaults to image centre."
    )
    args = parser.parse_args()

    root = Path(__file__).resolve().parent
    image_path = Path(args.image).expanduser()
    if not image_path.is_absolute():
        image_path = root / image_path

    model_path = Path(args.model).expanduser()
    if not model_path.is_absolute():
        model_path = root / model_path

    if not image_path.is_file():
        raise FileNotFoundError(f"Image not found: {image_path}")
    if not model_path.is_file():
        raise FileNotFoundError(f"YOLO model not found: {model_path}")

    original = cv2.imread(str(image_path))
    if original is None:
        raise ValueError(f"Could not read image: {image_path}")

    height, width = original.shape[:2]
    if args.center:
        junction_x, junction_y = args.center
    else:
        junction_x, junction_y = width / 2.0, height / 2.0

    print(f"Image dimensions: {width} x {height}")
    print(f"Junction centre: ({junction_x:.1f}, {junction_y:.1f})")
    print(
        f"Detection settings: confidence={args.confidence}, "
        f"tile={TILE_SIZE}, overlap={OVERLAP}, imgsz={IMAGE_SIZE}"
    )
    print("Loading YOLO model...")

    model = YOLO(str(model_path))
    raw_detections = []

    # Run on overlapping crops. Small/distant vehicles occupy more pixels
    # relative to each crop than they do in a single full-image inference.
    tiles = list(make_tiles(original))
    print(f"Processing {len(tiles)} overlapping image tile(s)...")

    for tile_number, (crop, x_offset, y_offset) in enumerate(tiles, start=1):
        print(f"  Tile {tile_number}/{len(tiles)}")
        raw_detections.extend(
            detect_on_crop(
                model, crop, x_offset, y_offset, args.confidence
            )
        )

    detections = class_aware_nms(raw_detections, width, height)

    lanes = {lane: [] for lane in LANES}
    lanes["Unassigned"] = []
    annotated = original.copy()
    details = []

    for detection in detections:
        x1, y1, x2, y2 = detection["bbox"]
        # Bottom-centre approximates where the vehicle touches the road.
        cx = (x1 + x2) / 2.0
        cy = y2
        lane = identify_lane(
            cx, cy, width, height, junction_x, junction_y
        )

        item = {
            "vehicle": detection["vehicle"],
            "lane": lane,
            "confidence": round(detection["confidence"], 4),
            "bbox": [round(v, 1) for v in [x1, y1, x2, y2]],
            "road_point": [round(cx, 1), round(cy, 1)]
        }
        lanes[lane].append(item)
        details.append(item)

        color = COLORS[lane]
        cv2.rectangle(
            annotated, (int(x1), int(y1)), (int(x2), int(y2)), color, 2
        )
        label = f'{item["vehicle"]} {lane} ({item["confidence"]:.2f})'
        cv2.putText(
            annotated, label,
            (int(x1), max(int(y1) - 7, 18)),
            cv2.FONT_HERSHEY_SIMPLEX, 0.48, color, 2, cv2.LINE_AA
        )

    # Sort approximately from nearest to the junction to farthest.
    for lane in LANES:
        lanes[lane].sort(
            key=lambda item: (
                (item["road_point"][0] - junction_x) ** 2
                + (item["road_point"][1] - junction_y) ** 2
            )
        )

    cv2.drawMarker(
        annotated, (int(junction_x), int(junction_y)),
        (255, 255, 255), cv2.MARKER_CROSS, 24, 2
    )
    cv2.putText(
        annotated, "Estimated junction centre",
        (int(junction_x) + 10, int(junction_y) - 10),
        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 2
    )

    print("\n========== VEHICLE COUNTS ==========")
    for lane in LANES + ["Unassigned"]:
        names = [item["vehicle"] for item in lanes[lane]]
        print(f"{lane}: {len(names)}  {names}")

    total_detected = len(details)
    print(f"\nTotal detected after duplicate removal: {total_detected}")

    y = 26
    for lane in LANES + ["Unassigned"]:
        cv2.putText(
            annotated, f'{lane}: {len(lanes[lane])}',
            (12, y), cv2.FONT_HERSHEY_SIMPLEX, 0.62,
            COLORS[lane], 2, cv2.LINE_AA
        )
        y += 27

    work = root / "work"
    work.mkdir(parents=True, exist_ok=True)

    detections_path = work / "lane_detections.txt"
    with detections_path.open("w", encoding="utf-8") as file:
        file.write("# Vehicles sorted approximately nearest to junction first\n")
        for lane in LANES:
            file.write(
                lane + ": " + " ".join(
                    item["vehicle"] for item in lanes[lane]
                ) + "\n"
            )

    image_out = work / "lane_visualization.jpg"
    if not cv2.imwrite(str(image_out), annotated):
        raise RuntimeError(f"Could not save annotated image: {image_out}")

    json_out = work / "lane_vehicle_detections.json"
    json_out.write_text(json.dumps({
        "source_image": image_path.name,
        "image_width": width,
        "image_height": height,
        "estimated_intersection_center": [junction_x, junction_y],
        "assignment_method": "image_relative_quadrants",
        "total_detected": total_detected,
        "lane_counts": {lane: len(lanes[lane]) for lane in LANES},
        "unassigned_count": len(lanes["Unassigned"]),
        "detections": details
    }, indent=2), encoding="utf-8")

    print(f"\nCreated: {detections_path}")
    print(f"Created: {image_out}")
    print(f"Created: {json_out}")
    print(
        "\nReminder: tiled inference can improve small-vehicle detection, "
        "but the model may still miss occluded or very small vehicles."
    )


if __name__ == "__main__":
    main()
