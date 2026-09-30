# Low-DPI clarity candidate (2026-09-30)

The existing default remains unhinted grayscale, gamma 0.85, contrast 1.0,
Mitchell filtering. This change does not claim to reproduce CoreText or fix all
reports of blur. Screen resolution alone is insufficient: record Windows scale,
physical em size, font/weight, application zoom, and whether the problem survives
a native-resolution screenshot.

## What changed

`LT_RENDER_CONFIG_HINTED_OUTLINES` now hints at the actual destination pixel size
before scaling the outline for Box/Mitchell supersampling. Previously those modes
hinted at 4x size, so their grid fitting was mostly lost on downsampling. Direct
and all unhinted paths keep their previous size, transform, phase, compensation,
and advance conversion. The glyph cache already distinguishes the hint flag.

The comparison app adds a candidate-only hint checkbox and snapshot flags
`--hinted` and `--filter direct|box|mitchell`. `--isolate-hint` matches the
right-side gamma/weight/blending to the left side for a controlled hinting A/B;
place it after `--dark` when combining those flags. The left side stays unhinted. The
filter selector remains shared, so compare separate snapshots to evaluate the
filter tradeoff. Reset restores the original unhinted setup. Command-line
render settings are for snapshots; the interactive window initializes defaults.

Hinting is an explicit alternative: it can sharpen small grid-aligned stems but
also change letter shapes and apparent weight. Mitchell still softens edges even
when hinting is enabled. Direct avoids that additional downsampling filter;
Box is a useful intermediate comparison. There is no automatic DPI threshold or
new global calibration in this patch.

## Try it on Windows

Build and run the complete Debug and Release CTest suites first:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DLUMATEXT_BUILD_TESTS=ON -DLUMATEXT_BUILD_SAMPLES=ON
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
.\build\samples\compare\Release\lumatext_compare.exe
```

In the compare
app, choose 12/14/16 DIP at 96/120/144/192 DPI; compare all three filters with the
candidate hint checkbox off/on. Match gamma and optical weight and disable
unified-background composition when isolating hinting from other differences.
Inspect regular/bold Chinese, Latin, punctuation, phases 0–7, and both themes.
Check the checkbox repeatedly, reset, resize, scroll, and move across monitors.

Example snapshot commands (right-side candidate is hinted):

```powershell
lumatext_compare.exe --snapshot hinted-direct-96.png --dpi 96 --size 14 --filter direct --hinted --isolate-hint
lumatext_compare.exe --snapshot hinted-box-120.png --dpi 120 --size 14 --filter box --hinted --isolate-hint
lumatext_compare.exe --snapshot hinted-mitchell-144.png --dpi 144 --size 14 --filter mitchell --hinted --isolate-hint
lumatext_compare.exe --snapshot hinted-dark-96.png --dpi 96 --size 14 --filter direct --hinted --dark --isolate-hint
```

Also capture the original unhinted 4K/150% setup before/after; its pixels should
be unchanged. Test in the real host, with its target DPI matching frame DPI and
without a second bitmap scaling pass. No renderer can recover detail lost if the
host or Windows first rasterizes at a lower resolution and then enlarges it.

## Verification available here

- Portable raster-scale test compiled as C++20 with `-Wall -Wextra -Werror
  -pedantic`: passed. Covers direct/supersampled, hinted/unhinted, phase units,
  physical compensation units, and advance conversion.
- FreeType 2.13.3 probe with local DejaVu Sans: 3,384 glyph loads across 10/13/16
  DIP and 96/120/144/192 DPI passed. Native and corrected supersampled hinted
  advances agreed; the previous 4x-hinting policy differed in 894 of 1,128 cases.
  This verifies the mechanism, not Microsoft YaHei appearance.
- Windows rasterizer regression cases added for hinted advance parity and all
  eight vertical phases with Direct/Box/Mitchell.
- Windows application build, Direct2D pixel tests, UI interactions, and actual
  monitor/CoreText comparisons were not run in the Linux editing environment.

Portable repro, with your FreeType development package and a local font:

```sh
c++ -std=c++20 -Wall -Wextra -Werror -pedantic -Isrc tests/raster_scale_test.cpp -o raster-scale-test
./raster-scale-test
c++ -std=c++20 -Isrc $(pkg-config --cflags freetype2) tests/raster_hinting_test.cpp $(pkg-config --libs freetype2) -o raster-hinting-test
./raster-hinting-test /path/to/font.ttf
```

## First Windows CI pass

Commit `b278610dbcbac61511047cfcd3f9e49f9fe4e326`, run
[36694714600](https://github.com/jimmgreen/lumatext/actions/runs/36694714600),
built both Debug and Release successfully. Both passed the new native-hinting
and scale tests and the extended rasterizer bounds tests. The same two failures
already present on main remained: the `fi` hit-test span and a color-emoji test
that required an isolated half-alpha pixel. The complete suite was not green.

The CoreText comparison job passed all 50 existing cases with unchanged
thresholds. Its 100 default rendered PNGs/masks were byte-for-byte identical to
main commit `7e280a41e9493a12f14f8126841779da093fc019` from run 36692359115.
That demonstrates default raster preservation for the tested matrix, not that
hinting candidates have been visually accepted on users' monitors.

The first hinting snapshot was produced, but the GUI executable exit-status
check stopped capture after it. The follow-up workflow explicitly waits for the
process and checks its own exit code. Timing labels in the comparison pictures
include initialization/cache effects and must not be used as A/B benchmarks.

## Baseline failures diagnosed separately

The script bridge passed DirectWrite's low-byte-first ISO script code directly
to HarfBuzz's high-byte-first tag. The bridge now converts explicitly, with
portable Latin/Arabic/Hebrew/Han/common/inherited tag tests. This is separate
from hinting and may correctly change script shaping outside the reference matrix.

Run [36696154692](https://github.com/jimmgreen/lumatext/actions/runs/36696154692)
verified both builds, the independent DirectWrite color oracle at foreground
opacity 1/0.5/0, all 24 comparison snapshots, and a runnable compare binary.
CoreText passed 50/50 and the same 100 default PNGs remained byte-identical for
this matrix. The one remaining suite failure was the installed-font fi assumption.

Run [36696890113](https://github.com/jimmgreen/lumatext/actions/runs/36696890113)
then directly shaped the runner's Segoe UI (955,804 bytes) using pinned HarfBuzz
10.4.0. It returned separate f/i/x glyphs for ar/en/und with liga both off and on;
LumaText matched all six cases. The font did not supply the substitution assumed
by the test. The final regression uses an owned, 1,144-byte synthetic rectangle
font with explicit fi GSUB, preserves the exact two-character hit-span assertions,
and adds liga-disabled one-character spans. No Windows font is redistributed.
Regenerate its checked-in header with `python tools/generate_ligature_test_font.py`
and fonttools 4.61.1. The final Windows suite must still be rerun on that commit.
