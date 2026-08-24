#pragma once

#include <lumatext/lumatext.h>

#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H
#include FT_OUTLINE_H
#include FT_TRUETYPE_TABLES_H

#include <hb.h>
#include <hb-ot.h>

#include <unicode/ubidi.h>
#include <unicode/ubrk.h>
#include <unicode/uscript.h>
#include <unicode/utf16.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <list>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lt {

using Microsoft::WRL::ComPtr;

constexpr uint64_t kObjectMagic = 0x4c554d4154455854ull;

struct Object {
  uint64_t magic = kObjectMagic;
  std::atomic<uint32_t> references{1};
  virtual ~Object() { magic = 0; }
  void retain() noexcept { references.fetch_add(1, std::memory_order_relaxed); }
  void release() noexcept {
    if (references.fetch_sub(1, std::memory_order_acq_rel) == 1) delete this;
  }
};

template <typename T>
bool descriptor_valid(const T* desc, size_t required_size) noexcept {
  return desc && desc->abi_version == LT_ABI_VERSION && desc->struct_size >= required_size;
}

inline float finite_or(float value, float fallback) noexcept {
  return std::isfinite(value) ? value : fallback;
}

struct FontBlob {
  struct AxisValue {
    uint32_t tag = 0;
    float value = 0.0f;
  };
  uint64_t identity = 0;
  uint32_t face_index = 0;
  uint16_t weight = 400;
  bool has_color = false;
  std::vector<uint8_t> bytes;
  std::vector<AxisValue> axes;
};

uint64_t next_font_identity() noexcept;

struct GlyphKey {
  uint64_t font_identity = 0;
  uint32_t glyph_index = 0;
  uint32_t em_size_26_6 = 0;
  uint16_t dpi_x = 96;
  uint16_t dpi_y = 96;
  uint8_t x_phase = 0;
  uint8_t gamma_64 = 64;
  uint8_t contrast_64 = 64;
  uint8_t stem_64 = 0;
  uint8_t synthetic_64 = 0;
  uint8_t hinted = 0;

  bool operator==(const GlyphKey&) const noexcept = default;
};

struct GlyphKeyHash {
  size_t operator()(const GlyphKey& key) const noexcept;
};

struct GlyphBitmap {
  int32_t left = 0;
  int32_t top = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  std::vector<uint8_t> pixels;
};

struct CachedGlyph {
  std::shared_ptr<const GlyphBitmap> bitmap;
  std::list<GlyphKey>::iterator lru_position;
};

class FontBridge {
 public:
  lt_result get_blob(IDWriteFontFace* face, std::shared_ptr<const FontBlob>& out);

 private:
  struct FaceEntry {
    ComPtr<IDWriteFontFace> face;
    std::shared_ptr<const FontBlob> blob;
  };
  std::mutex mutex_;
  std::unordered_map<IDWriteFontFace*, FaceEntry> cache_;
};

class Rasterizer {
 public:
  static lt_result render(std::shared_ptr<const FontBlob> font, const GlyphKey& key,
                          std::shared_ptr<const GlyphBitmap>& out);
};

struct ShapedGlyph {
  uint32_t glyph_index = 0;
  uint32_t cluster = 0;
  float x = 0.0f;
  float y = 0.0f;
  float advance = 0.0f;
};

struct ShapedRun {
  lt_font_face* face = nullptr;
  std::vector<ShapedGlyph> glyphs;
  float font_size = 0.0f;
  uint16_t weight = 400;
  uint32_t style_flags = 0;
  uint32_t text_start = 0;
  uint32_t text_length = 0;
  bool right_to_left = false;
  bool color = false;
  bool synthetic_bold = false;
};

struct ClusterBox {
  uint32_t text_position = 0;
  uint32_t text_length = 0;
  float left = 0.0f;
  float width = 0.0f;
  bool right_to_left = false;
};

struct OwnedStyle {
  lt_font_cascade* cascade = nullptr;
  float font_size = 14.0f;
  float letter_spacing = 0.0f;
  uint16_t weight = 400;
  uint32_t flags = 0;
  std::vector<lt_open_type_feature> features;
};

}  // namespace lt

struct lt_context final : lt::Object {
  lt::ComPtr<IDWriteFactory> dwrite_factory;
  lt_log_callback log_callback = nullptr;
  void* log_user_data = nullptr;
  lt_glyph_ready_callback glyph_ready_callback = nullptr;
  void* glyph_ready_user_data = nullptr;
  uint64_t cpu_cache_limit = 32ull * 1024ull * 1024ull;
  lt::FontBridge font_bridge;

  std::mutex glyph_mutex;
  std::unordered_map<lt::GlyphKey, lt::CachedGlyph, lt::GlyphKeyHash> glyph_cache;
  std::list<lt::GlyphKey> glyph_lru;
  uint64_t glyph_cache_bytes = 0;

  void log(int32_t level, const char* message) const noexcept;
  lt_result get_glyph(IDWriteFontFace* face, const lt::GlyphKey& key,
                      std::shared_ptr<const lt::GlyphBitmap>& out, bool* cache_hit = nullptr);
  lt_result get_glyph(std::shared_ptr<const lt::FontBlob> font, const lt::GlyphKey& key,
                      std::shared_ptr<const lt::GlyphBitmap>& out, bool* cache_hit = nullptr);
};

struct lt_font_face final : lt::Object {
  lt_context* context = nullptr;
  std::shared_ptr<const lt::FontBlob> blob;
  std::wstring source_path;

  ~lt_font_face() override;
};

struct lt_font_cascade final : lt::Object {
  struct Entry {
    lt_font_face* face = nullptr;
    uint16_t weight = 400;
  };
  lt_context* context = nullptr;
  std::vector<Entry> entries;
  bool allow_system_fallback = false;
  std::wstring system_fallback_family;

  ~lt_font_cascade() override;
};

struct lt_render_profile final : lt::Object {
  lt_render_config light{};
  lt_render_config dark{};
  float regular_optical_weight = 0.0f;
  float bold_optical_weight = 0.0f;
  uint32_t flags = 0;
};

struct lt_text_layout final : lt::Object {
  lt_context* context = nullptr;
  std::u16string text;
  std::vector<lt::ShapedRun> runs;
  std::vector<lt::ClusterBox> clusters;
  std::vector<lt_font_face*> retained_faces;
  lt_text_metrics metrics{};
  float alignment_offset = 0.0f;
  uint64_t shaping_time_us = 0;

  ~lt_text_layout() override;
};

enum class lt_renderer_kind { d2d };

struct lt_renderer final : lt::Object {
  lt_context* context = nullptr;
  lt_renderer_kind kind = lt_renderer_kind::d2d;
  lt::ComPtr<ID2D1RenderTarget> d2d_target;
  DWORD owner_thread = 0;
  bool manage_begin_end_draw = false;
  bool frame_active = false;

  ~lt_renderer() override;
};

struct lt_frame final : lt::Object {
  lt_renderer* renderer = nullptr;
  float dpi_x = 96.0f;
  float dpi_y = 96.0f;
  bool ended = false;
  bool owns_draw = false;
  lt_frame_stats stats{};

  ~lt_frame() override;
};
