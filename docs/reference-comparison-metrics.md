# Reference comparison metrics

`tools/compare_reference.py` compares the non-fallback cases declared by the
CoreText and LumaText manifests. A successful aggregate report requires a
nonempty, matching set of case keys and every compared case to satisfy the
existing numerical thresholds. Keys are name, background, and scale rounded
to two decimal places. Duplicate keys are rejected rather than overwritten.
`comparisonCoverage` lists cases missing from either manifest. This validates
agreement between the supplied manifests, not completeness against an external
acceptance matrix: a case absent from both manifests cannot be detected here.

## Raster metrics version 2

The existing `horizontalStrokePx` and `verticalStrokePx` field names are kept
for consumers, but their previous first-to-last ink-span calculation was
incorrect: it counted gaps as ink and discarded one-pixel runs. Version 2
measures the mean length of contiguous runs at alpha >= 0.5, along rows and
columns respectively. Every run has equal weight. Single-pixel runs count;
spaces and gaps do not. The result describes scan runs, not anatomical stem
widths: glyph shape and stroke intersections still affect it. Do not compare
version 2 values with historical span values as if they were the same metric.

`haloPixels` remains a count of pixels with 0 < alpha < 0.08. It does not by
itself establish an unwanted visual halo. Edge alpha, low-alpha counts, run
lengths and axis differences are descriptive diagnostics, not pass/fail gates.
No new aesthetic acceptance thresholds were introduced. The existing width,
glyph-origin, coverage and SSIM thresholds are unchanged. Real-display A/B
review remains necessary.

## Portable regression tests

Install `numpy`, `Pillow` and `scikit-image` in a Python environment, then run:

```sh
python -W error tests/compare_reference_test.py
```

The tests cover separated and one-pixel stems, scan-axis transposition,
coverage thresholds, empty masks, duplicate/missing cases, fallback exclusions,
and complete/incomplete comparison reports. CLI tests use actual SSIM and PNG
files; they explicitly skip when scikit-image is unavailable. These tests do
not render fonts and do not replace the Windows rendering suite.
