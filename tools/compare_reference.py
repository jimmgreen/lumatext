#!/usr/bin/env python3
import argparse
import json
import math
import pathlib
import sys

import numpy as np
from PIL import Image
from skimage.metrics import structural_similarity


LIGHT_BG = np.array([0.965, 0.969, 0.973], dtype=np.float32)
DARK_BG = np.array([0.09, 0.098, 0.11], dtype=np.float32)
LIGHT_FG = np.array([0.09, 0.098, 0.11], dtype=np.float32)
DARK_FG = np.array([0.91, 0.918, 0.929], dtype=np.float32)


def coverage(image_path: pathlib.Path, background: str) -> np.ndarray:
    image = np.asarray(Image.open(image_path).convert("RGB"), dtype=np.float32) / 255.0
    bg = LIGHT_BG if background == "light" else DARK_BG
    fg = LIGHT_FG if background == "light" else DARK_FG
    denominator = max(float(np.mean(np.abs(fg - bg))), 1e-6)
    return np.clip(np.mean(np.abs(image - bg), axis=2) / denominator, 0.0, 1.0)


def shifted(source: np.ndarray, dx: int, dy: int) -> np.ndarray:
    output = np.zeros_like(source)
    source_y0 = max(0, -dy)
    source_y1 = min(source.shape[0], source.shape[0] - dy)
    source_x0 = max(0, -dx)
    source_x1 = min(source.shape[1], source.shape[1] - dx)
    if source_y1 > source_y0 and source_x1 > source_x0:
        output[source_y0 + dy:source_y1 + dy, source_x0 + dx:source_x1 + dx] = \
            source[source_y0:source_y1, source_x0:source_x1]
    return output


def align(reference: np.ndarray, candidate: np.ndarray):
    best = None
    for dy in range(-2, 3):
        for dx in range(-2, 3):
            moved = shifted(candidate, dx, dy)
            error = float(np.mean(np.square(reference - moved)))
            if best is None or error < best[0]:
                best = (error, dx, dy, moved)
    return best[1], best[2], best[3]


def record_key(record):
    return record["name"], record["background"], round(float(record["scale"]), 2)


def glyph_origin_errors(reference, candidate, scale):
    reference_glyphs = reference.get("glyphs", [])
    candidate_glyphs = candidate.get("glyphs", [])
    count = min(len(reference_glyphs), len(candidate_glyphs))
    if count == 0:
        return math.inf, math.inf
    errors = []
    for index in range(count):
        left = reference_glyphs[index]
        right = candidate_glyphs[index]
        errors.append(math.hypot(float(left["x"]) - float(right["x"]),
                                 float(left["y"]) - float(right["y"])) * scale)
    if len(reference_glyphs) != len(candidate_glyphs):
        errors.extend([1.0] * abs(len(reference_glyphs) - len(candidate_glyphs)))
    values = np.asarray(errors, dtype=np.float64)
    return float(np.sqrt(np.mean(values * values))), float(np.max(values))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--coretext", required=True, type=pathlib.Path)
    parser.add_argument("--lumatext", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    overlays = args.output / "overlays"
    overlays.mkdir(exist_ok=True)

    core_manifest = json.loads((args.coretext / "manifest.json").read_text("utf-8"))
    luma_manifest = json.loads((args.lumatext / "manifest.json").read_text("utf-8"))
    if core_manifest["regularSHA256"] != luma_manifest["regularSHA256"] or \
       core_manifest["boldSHA256"] != luma_manifest["boldSHA256"]:
        raise RuntimeError("font hashes differ between CoreText and LumaText artifacts")

    core_records = {record_key(value): value for value in core_manifest["records"]
                    if not value.get("fallbackCase", False)}
    luma_records = {record_key(value): value for value in luma_manifest["records"]
                    if not value.get("fallbackCase", False)}
    results = []
    failed = False
    for key in sorted(core_records.keys() & luma_records.keys()):
        core = core_records[key]
        luma = luma_records[key]
        reference = coverage(args.coretext / core["image"], core["background"])
        candidate = coverage(args.lumatext / luma["image"], luma["background"])
        if reference.shape != candidate.shape:
            raise RuntimeError(f"image dimensions differ for {key}")
        dx, dy, candidate = align(reference, candidate)
        ssim = float(structural_similarity(reference, candidate, data_range=1.0))
        reference_total = max(float(np.sum(reference)), 1e-6)
        coverage_difference = abs(float(np.sum(candidate)) - reference_total) / reference_total
        scale = float(core["scale"])
        line_width_error = abs(float(core["width"]) - float(luma["width"])) * scale
        origin_rms, origin_max = glyph_origin_errors(core, luma, scale)
        passed = (line_width_error <= 0.5 and origin_rms <= 0.2 and
                  origin_max <= 0.5 and coverage_difference <= 0.03 and ssim >= 0.97)
        failed = failed or not passed
        result = {
            "name": core["name"], "background": core["background"], "scale": scale,
            "alignment": {"dx": dx, "dy": dy}, "lineWidthErrorPx": line_width_error,
            "glyphOriginRMSPx": origin_rms, "glyphOriginMaxPx": origin_max,
            "coverageDifference": coverage_difference, "ssim": ssim, "passed": passed,
        }
        results.append(result)
        overlay = np.zeros((*reference.shape, 3), dtype=np.float32)
        overlay[:, :, 0] = reference
        overlay[:, :, 1] = candidate
        overlay[:, :, 2] = candidate
        Image.fromarray(np.uint8(np.clip(overlay, 0, 1) * 255), "RGB").save(
            overlays / core["image"])

    report = {
        "thresholds": {"lineWidthPx": 0.5, "glyphOriginRMSPx": 0.2,
                       "glyphOriginMaxPx": 0.5, "coverageDifference": 0.03,
                       "ssim": 0.97},
        "coretext": {"osBuild": core_manifest["osBuild"],
                     "regularSHA256": core_manifest["regularSHA256"],
                     "boldSHA256": core_manifest["boldSHA256"]},
        "results": results,
        "passed": not failed and bool(results),
    }
    (args.output / "comparison.json").write_text(
        json.dumps(report, indent=2, sort_keys=True), "utf-8")
    print(json.dumps(report, indent=2, sort_keys=True))
    return 1 if failed or not results else 0


if __name__ == "__main__":
    sys.exit(main())
