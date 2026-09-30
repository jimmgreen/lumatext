"""Portable QA regression tests. Run: python tests/compare_reference_test.py.

Requires numpy and Pillow; CLI integration tests additionally require scikit-image.
"""
import contextlib
import importlib.util
import io
import json
import pathlib
import sys
import tempfile
import unittest
from unittest import mock

import numpy as np
from PIL import Image

MODULE_PATH = pathlib.Path(__file__).resolve().parents[1] / "tools" / "compare_reference.py"
SPEC = importlib.util.spec_from_file_location("compare_reference", MODULE_PATH)
compare = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(compare)


class RasterMetricsTest(unittest.TestCase):
    def test_disjoint_stems_do_not_include_gaps(self):
        mask = np.zeros((5, 12), dtype=np.float32)
        mask[:, 1] = 1
        mask[:, 8:10] = 1
        metrics = compare.raster_metrics(mask)
        self.assertEqual(metrics["horizontalStrokePx"], 1.5)
        self.assertEqual(metrics["verticalStrokePx"], 5)

    def test_single_pixel_is_a_run(self):
        metrics = compare.raster_metrics(np.ones((1, 1), dtype=np.float32))
        self.assertEqual(metrics["horizontalStrokePx"], 1)
        self.assertEqual(metrics["verticalStrokePx"], 1)

    def test_transpose_exchanges_scan_axes(self):
        mask = np.array([[1, 1, 0, 1], [0, 1, 0, 1]], dtype=np.float32)
        original = compare.raster_metrics(mask)
        transposed = compare.raster_metrics(mask.T)
        self.assertEqual(original["horizontalStrokePx"], transposed["verticalStrokePx"])
        self.assertEqual(original["verticalStrokePx"], transposed["horizontalStrokePx"])

    def test_core_threshold_and_no_core(self):
        metrics = compare.raster_metrics(np.array([[0.49, 0.5, 0.49]], dtype=np.float32))
        self.assertEqual(metrics["horizontalStrokePx"], 1)
        metrics = compare.raster_metrics(np.full((2, 2), 0.25, dtype=np.float32))
        self.assertEqual(metrics["horizontalStrokePx"], 0)
        self.assertEqual(metrics["verticalStrokePx"], 0)

    def test_empty_mask(self):
        self.assertTrue(all(value == 0 for value in
                            compare.raster_metrics(np.zeros((4, 4))).values()))


def record(name="body", **extra):
    return {"name": name, "background": "light", "scale": 1,
            "mask": "mask.png", "image": "image.png", "width": 8,
            "glyphs": [{"x": 0, "y": 0}], **extra}


class ComparisonCoverageTest(unittest.TestCase):
    def test_missing_records_report_both_directions(self):
        reference = compare.comparable_records({"records": [record("shared"), record("reference-only")]})
        candidate = compare.comparable_records({"records": [record("shared"), record("candidate-only")]})
        gaps = compare.missing_records(reference, candidate)
        self.assertEqual(gaps["missingFromLumaText"][0]["name"], "reference-only")
        self.assertEqual(gaps["missingFromCoreText"][0]["name"], "candidate-only")

    def test_fallback_exclusion_is_preserved(self):
        records = compare.comparable_records({"records": [record(), record("emoji", fallbackCase=True)]})
        self.assertEqual(len(records), 1)

    def test_duplicates_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "duplicate comparison record"):
            compare.comparable_records({"records": [record(), record()]})


class CommandLineTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        try:
            from skimage.metrics import structural_similarity
        except ImportError as error:
            raise unittest.SkipTest(f"CLI integration requires scikit-image: {error}")

    def run_comparison(self, reference_records, candidate_records):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            for name, records in [("core", reference_records), ("luma", candidate_records)]:
                directory = root / name
                directory.mkdir()
                manifest = {"osBuild": "test", "regularSHA256": "regular",
                            "boldSHA256": "bold", "records": records}
                (directory / "manifest.json").write_text(json.dumps(manifest), "utf-8")
                pixels = np.zeros((16, 16, 4), dtype=np.uint8)
                pixels[3:12, 5:7, 3] = 255
                Image.fromarray(pixels).save(directory / "mask.png")
            argv = [str(MODULE_PATH), "--coretext", str(root / "core"),
                    "--lumatext", str(root / "luma"), "--output", str(root / "out")]
            with mock.patch.object(sys, "argv", argv), contextlib.redirect_stdout(io.StringIO()):
                result = compare.main()
            report = json.loads((root / "out" / "comparison.json").read_text("utf-8"))
            return result, report

    def test_complete_identical_case_passes(self):
        result, report = self.run_comparison([record()], [record()])
        self.assertEqual(result, 0)
        self.assertTrue(report["passed"])
        self.assertEqual(report["rasterMetricsVersion"], 2)
        self.assertEqual(report["comparisonCoverage"]["comparedCases"], 1)
        self.assertEqual(report["thresholds"]["ssim"], 0.97)
        self.assertEqual(report["thresholds"]["coverageDifference"], 0.03)

    def test_missing_case_fails_even_if_shared_case_passes(self):
        for reference, candidate, missing in [
            ([record(), record("missing")], [record()], "missingFromLumaText"),
            ([record()], [record(), record("missing")], "missingFromCoreText"),
        ]:
            with self.subTest(missing=missing):
                result, report = self.run_comparison(reference, candidate)
                self.assertEqual(result, 1)
                self.assertFalse(report["passed"])
                self.assertTrue(report["results"][0]["passed"])
                self.assertEqual(len(report["comparisonCoverage"][missing]), 1)

    def test_no_shared_or_empty_cases_fail(self):
        for reference, candidate in [([], []), ([record("a")], [record("b")])]:
            with self.subTest(reference=reference, candidate=candidate):
                result, report = self.run_comparison(reference, candidate)
                self.assertEqual(result, 1)
                self.assertFalse(report["passed"])
                self.assertEqual(report["results"], [])


if __name__ == "__main__":
    unittest.main()
