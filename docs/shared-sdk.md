# Shared Windows DLL SDK

Run `build-sdk.bat` from the original LumaText project. It builds and tests x64 Debug and Release, then packages both under `out/sdk`.

| Configuration | DLL | Import library | MSVC runtime |
| --- | --- | --- | --- |
| Debug | `Debug/bin/lumatextd.dll` | `Debug/lib/lumatextd.lib` | `/MTd` |
| Release | `Release/bin/lumatext.dll` | `Release/lib/lumatext.lib` | `/MT` |

For each consuming project:

1. Add the selected configuration's `include` directory to the compiler include path.
2. Link its matching import library. Do not define `LUMATEXT_STATIC` or `LUMATEXT_BUILD_DLL`.
3. Copy its DLL beside the application executable. Keep headers, import library and DLL from the same SDK build.

Debug includes a PDB. By default the CRT is embedded in each DLL, so Release needs no separate MSVC runtime DLLs. CMake consumers can explicitly set CMAKE_MSVC_RUNTIME_LIBRARY to select a different runtime; the package manifest records the actual choice. Release builds using MultiThreadedDLL still require the matching Microsoft Visual C++ x64 runtime. Release opaque handles only through lt_release; never free them in the consumer. FreeType and HarfBuzz are embedded in the DLL. No LumaText source copy is required by consumers.

Each configuration includes licenses and `manifest.json` with file sizes and SHA-256 hashes. After fixing the original library, run the script again and update consumers' matching SDK artifacts.

For repeated glyph requests, create an `lt_font_face` once with
`lt_font_face_create` and set the optional `lt_glyph_request.font_face` field.
The face must belong to the request's context and remain alive during the call;
release it with `lt_release` when finished. Memory sources are copied at face
creation, so their original buffers can be freed immediately afterward. Glyph
images retain their bitmap independently of the face handle.

The preloaded face takes precedence over `dwrite_face` and `font_bytes`, and uses
the face index and variation axes selected at creation. It avoids hashing the
entire input font on every request. Requests without a face preserve the existing
DirectWrite-first, then memory-bytes behavior. Existing descriptor prefixes remain
valid: the new pointer is read only when `struct_size` includes the entire field.
Initialize new requests with the current structure size and ABI version (for C++,
`LumaText::Descriptor<lt_glyph_request>()`). Use matching updated headers and DLLs
when opting into the new field.

`lumatext_glyph_provider_benchmark` compares warmed memory-byte and preloaded-face
lookups with the same font, glyph sequence, size, DPI, and render settings. It
reports time per call without a machine-dependent pass/fail performance threshold.

The single-line renderer automatically caches composed non-color text bitmaps per
renderer (LRU, at most 64 entries and 8 MiB of pixel payload). Reuse the same
`lt_text_layout` for repeated draws. Layout identity, exact origin, DPI, colors,
background mode, and effective render configuration participate in the key;
clipping is applied when drawing. Color-font runs keep their existing path.
Changing DPI, replacing the render target, or detecting device loss clears the
cache. `lt_d2d_renderer_set_target` must be called on the owner thread outside an
active frame. Cached images do not retain layouts or their font cascades.

`lt_frame_stats.line_cache_hits` counts reused line images. Such draws do not
perform glyph-cache lookups, CPU composition, or bitmap uploads. Older stats
descriptor sizes remain supported. `lumatext_line_cache_test --benchmark` measures
120 repeated frames of 12 lines on a WIC software target; it is not a GPU or host
application frame-rate benchmark.

## Release size optimization

MSVC Release and MinSizeRel builds enable link-time optimization and `/Gw` for LumaText and its bundled FreeType/HarfBuzz dependencies. `/Os` is limited to the bundled dependencies; the renderer retains the Release speed optimization defaults. All font modules and
the public ABI remain available; Debug remains unoptimized. Disable this with
`-DLUMATEXT_OPTIMIZE_RELEASE_SIZE=OFF` for baseline comparisons or when building
static libraries intended for consumers using another MSVC toolset. IPO static
objects require a compatible compiler/linker toolset; the DLL C ABI is unchanged.
The SDK script still builds both library forms to retain the existing static-only tests; the shared SDK package does not include the static library.