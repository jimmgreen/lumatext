#include <lumatext/lumatext.hpp>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <dwrite_3.h>
#include <wincodec.h>
#include <shellapi.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <sstream>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
namespace {
constexpr wchar_t sample[] = L"微软雅黑：清晰的文字，安静的阅读。Aa Bb 0123456789";
enum { Theme = 101, Size, Dpi, Filter, Gamma, Regular, Bold, Phase, Reset, Text, Blend, Hint };
enum class NativeMode { None, Grayscale, ClearType };
struct App {
  ComPtr<ID2D1Factory> factory;
  ComPtr<IDWriteFactory> write;
  ComPtr<ID2D1HwndRenderTarget> windowTarget;
  ComPtr<IDWriteTextFormat> label;
  LumaText::Context context;
  LumaText::FontFace regular, bold;
  LumaText::FontCascade cascade;
  LumaText::RenderProfile profile;
  LumaText::Renderer renderer;
  std::array<LumaText::TextLayout, 8> layouts;
  NativeMode nativeMode = NativeMode::None;
  ComPtr<IDWriteFontCollection1> nativeCollection;
  ComPtr<IDWriteRenderingParams> nativeParams;
  std::array<std::wstring, 2> fontPaths, nativeFamilies;
  std::array<UINT32, 2> nativeFamilyIndices{};
  std::array<ComPtr<IDWriteTextLayout>, 8> nativeLayouts;
  std::array<float, 8> nativeBaselineShift{}, lumaAscent{};
  std::array<HWND, 12> controls{};
  std::vector<HWND> captions;
  HFONT controlFont = nullptr;
  bool dark = false;
  float size = 16, dpi = 96, gamma = .9f, regularWeight = .02f, boldWeight = 0;
  int filter = LT_RASTER_FILTER_MITCHELL, phase = 0;
  float layoutWidth = -1;
  std::wstring text = sample;
  bool dirty = true, updating = false;
  bool blend = true;
  bool hinted = false;
  int scroll = 0;
  ~App() { if (controlFont) DeleteObject(controlFont); }
} app;

const wchar_t* native_name() {
  return app.nativeMode == NativeMode::ClearType ? L"ClearType RGB" : L"grayscale";
}
const wchar_t* filter_name() {
  return app.filter == LT_RASTER_FILTER_DIRECT ? L"direct" :
    app.filter == LT_RASTER_FILTER_BOX ? L"box" : L"Mitchell";
}
bool native_face_matches(IDWriteFontFace* face, size_t weight) {
  if (!face || face->GetIndex() != 0 || face->GetSimulations() != DWRITE_FONT_SIMULATIONS_NONE) return false;
  UINT32 count = 0;
  ComPtr<IDWriteFontFile> file;
  ComPtr<IDWriteFontFileLoader> loader;
  ComPtr<IDWriteLocalFontFileLoader> local;
  const void* key = nullptr; UINT32 keySize = 0, length = 0;
  if (FAILED(face->GetFiles(&count, nullptr)) || count != 1 || FAILED(face->GetFiles(&count, &file)) ||
      FAILED(file->GetReferenceKey(&key, &keySize)) || FAILED(file->GetLoader(&loader)) ||
      FAILED(loader.As(&local)) || FAILED(local->GetFilePathLengthFromKey(key, keySize, &length))) return false;
  std::wstring path(static_cast<size_t>(length) + 1, L'\0');
  return SUCCEEDED(local->GetFilePathFromKey(key, keySize, path.data(), length + 1)) &&
    _wcsicmp(path.c_str(), app.fontPaths[weight].c_str()) == 0;
}
// Validate actual shaped runs, not merely the family requested on the format.
// Reject fallback faces and simulations rather than silently comparing other fonts.
class NativeFaceVerifier final : public IDWriteTextRenderer {
 public:
  explicit NativeFaceVerifier(size_t weight) : weight_(weight) {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
    if (!out) return E_POINTER;
    *out = nullptr;
    if (iid == __uuidof(IUnknown) || iid == __uuidof(IDWritePixelSnapping) || iid == __uuidof(IDWriteTextRenderer)) {
      *out = static_cast<IDWriteTextRenderer*>(this); AddRef(); return S_OK;
    }
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
  ULONG STDMETHODCALLTYPE Release() override { return 1; }
  HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*, BOOL* disabled) override { *disabled = TRUE; return S_OK; }
  HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*, DWRITE_MATRIX* matrix) override {
    *matrix = {1, 0, 0, 1, 0, 0}; return S_OK;
  }
  HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* value) override { *value = app.dpi / 96; return S_OK; }
  HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*, FLOAT, FLOAT, DWRITE_MEASURING_MODE,
      const DWRITE_GLYPH_RUN* run, const DWRITE_GLYPH_RUN_DESCRIPTION*, IUnknown*) override {
    if (!run || !native_face_matches(run->fontFace, weight_)) return E_FAIL;
    for (UINT32 i = 0; i < run->glyphCount; ++i) if (!run->glyphIndices[i]) return E_FAIL;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE DrawUnderline(void*, FLOAT, FLOAT, const DWRITE_UNDERLINE*, IUnknown*) override { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*, FLOAT, FLOAT, const DWRITE_STRIKETHROUGH*, IUnknown*) override { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE DrawInlineObject(void*, FLOAT, FLOAT, IDWriteInlineObject*, BOOL, BOOL, IUnknown*) override { return E_NOTIMPL; }
 private:
  size_t weight_;
};
bool initialize_native() {
  ComPtr<IDWriteFactory3> factory;
  ComPtr<IDWriteFontSetBuilder> builder;
  ComPtr<IDWriteFontSet> set;
  std::array<ComPtr<IDWriteFontFace3>, 2> faces;
  if (FAILED(app.write.As(&factory)) || FAILED(factory->CreateFontSetBuilder(&builder))) return false;
  for (size_t i = 0; i < faces.size(); ++i) {
    ComPtr<IDWriteFontFaceReference> reference;
    if (FAILED(factory->CreateFontFaceReference(app.fontPaths[i].c_str(), nullptr, 0,
          DWRITE_FONT_SIMULATIONS_NONE, &reference)) || FAILED(reference->CreateFontFace(&faces[i])) ||
        FAILED(builder->AddFontFaceReference(reference.Get()))) return false;
  }
  if (FAILED(builder->CreateFontSet(&set)) || FAILED(factory->CreateFontCollectionFromFontSet(set.Get(), &app.nativeCollection))) return false;
  for (size_t i = 0; i < faces.size(); ++i) {
    ComPtr<IDWriteFont> font, matched;
    ComPtr<IDWriteFontFamily> family;
    ComPtr<IDWriteLocalizedStrings> names;
    ComPtr<IDWriteFontFace> matchedFace;
    UINT32 index = 0, length = 0; BOOL exists = FALSE;
    if (FAILED(app.nativeCollection->GetFontFromFontFace(faces[i].Get(), &font)) ||
        FAILED(font->GetFontFamily(&family)) || FAILED(family->GetFamilyNames(&names)) ||
        FAILED(names->FindLocaleName(L"en-us", &index, &exists))) return false;
    if (!exists) index = 0;
    if (FAILED(names->GetStringLength(index, &length))) return false;
    std::wstring name(static_cast<size_t>(length) + 1, L'\0');
    if (FAILED(names->GetString(index, name.data(), length + 1))) return false;
    name.resize(length); app.nativeFamilies[i] = std::move(name);
    if (FAILED(app.nativeCollection->FindFamilyName(app.nativeFamilies[i].c_str(), &app.nativeFamilyIndices[i], &exists)) || !exists ||
        FAILED(family->GetFirstMatchingFont(i ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
          DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &matched)) ||
        FAILED(matched->CreateFontFace(&matchedFace)) || !native_face_matches(matchedFace.Get(), i)) return false;
  }
  ComPtr<IDWriteRenderingParams> defaults;
  // Retain native gamma/contrast and recommended per-size rendering-mode selection.
  // Force RGB geometry + full ClearType so headless CI cannot silently choose FLAT.
  return SUCCEEDED(app.write->CreateRenderingParams(&defaults)) &&
    SUCCEEDED(app.write->CreateCustomRenderingParams(defaults->GetGamma(), defaults->GetEnhancedContrast(),
      1.0f, DWRITE_PIXEL_GEOMETRY_RGB, DWRITE_RENDERING_MODE_DEFAULT, &app.nativeParams));
}

lt_render_config config(bool candidate) {
  auto c = LumaText::Descriptor<lt_render_config>();
  c.coverage_gamma = candidate ? app.gamma : .85f;
  c.coverage_contrast = 1;
  c.raster_filter = static_cast<uint8_t>(app.filter);
  c.flags = candidate && app.blend ? LT_RENDER_CONFIG_LINEAR_BLEND | LT_RENDER_CONFIG_KNOWN_BACKGROUND : 0;
  if (candidate && app.hinted) c.flags |= LT_RENDER_CONFIG_HINTED_OUTLINES;
  return c;
}
bool rebuild(float width) {
  if (!app.dirty && app.layoutWidth == width) return true;
  app.profile.reset();
  auto p = LumaText::Descriptor<lt_render_profile_desc>();
  p.light = p.dark = config(true);
  p.regular_optical_weight = app.regularWeight;
  p.bold_optical_weight = app.boldWeight;
  if (lt_render_profile_create(&p, app.profile.put()) != LT_OK) return false;
  const std::array<std::wstring, 4> lines = {
    app.text, L"永 国 重 最 美 微 软 雅 黑，标点。、：；！？",
    L"Hamburgefontsiv  Il1 O0  AV To  fi fl  0123456789",
    L"阅读与设计 / Windows / macOS / 细节决定体验"};
  for (size_t i = 0; i < app.layouts.size(); ++i) {
    app.layouts[i].reset();
    auto s = LumaText::Descriptor<lt_text_style>();
    s.cascade = app.cascade.get(); s.font_size = app.size; s.weight = i < 4 ? 400 : 700;
    auto d = LumaText::Descriptor<lt_text_layout_desc>();
    d.text = lines[i % 4].c_str(); d.text_length = static_cast<uint32_t>(lines[i % 4].size());
    d.base_style = s; d.locale = "zh-CN"; d.max_width = width;
    d.ellipsis = app.nativeMode == NativeMode::None ? LT_TEXT_ELLIPSIS_END : LT_TEXT_ELLIPSIS_NONE;
    if (lt_text_layout_create(app.context.get(), &d, app.layouts[i].put()) != LT_OK) return false;
    if (app.nativeMode != NativeMode::None) {
      const size_t weight = i < 4 ? 0 : 1;
      ComPtr<IDWriteTextFormat> format;
      app.nativeLayouts[i].Reset();
      if (FAILED(app.write->CreateTextFormat(app.nativeFamilies[weight].c_str(), app.nativeCollection.Get(),
            weight ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, app.size, L"zh-CN", &format)) ||
          FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP)) ||
          FAILED(app.write->CreateTextLayout(d.text, d.text_length, format.Get(), width, app.size * 4, &app.nativeLayouts[i]))) return false;
      DWRITE_LINE_METRICS line{}; UINT32 lineCount = 0;
      auto metrics = LumaText::Descriptor<lt_text_metrics>();
      NativeFaceVerifier verifier(weight);
      if (FAILED(app.nativeLayouts[i]->GetLineMetrics(&line, 1, &lineCount)) || lineCount != 1 ||
          lt_text_layout_get_metrics(app.layouts[i].get(), &metrics) != LT_OK ||
          FAILED(app.nativeLayouts[i]->Draw(nullptr, &verifier, 0, 0))) return false;
      app.lumaAscent[i] = metrics.ascent;
      app.nativeBaselineShift[i] = metrics.ascent - line.baseline;
    }
  }
  app.layoutWidth = width; app.dirty = false;
  return true;
}
bool attach(ID2D1RenderTarget* target) {
  app.renderer.reset();
  auto d = LumaText::Descriptor<lt_d2d_desc>();
  d.render_target = target; d.manage_begin_end_draw = false;
  return lt_d2d_renderer_create(app.context.get(), &d, app.renderer.put()) == LT_OK;
}
void label(ID2D1RenderTarget* t, ID2D1SolidColorBrush* b, const std::wstring& s,
           float x, float y, float w, float h = 30) {
  t->DrawTextW(s.c_str(), static_cast<UINT32>(s.size()), app.label.Get(),
    D2D1::RectF(x, y, x + w, y + h), b, D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
void draw_native(ID2D1RenderTarget* t, ID2D1SolidColorBrush* brush, size_t index,
                 D2D1_POINT_2F origin, NativeMode mode) {
  const auto previousAA = t->GetTextAntialiasMode();
  ComPtr<IDWriteRenderingParams> previousParams;
  t->GetTextRenderingParams(&previousParams);
  t->SetTextRenderingParams(app.nativeParams.Get());
  t->SetTextAntialiasMode(mode == NativeMode::ClearType ? D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE : D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
  origin.y += app.nativeBaselineShift[index];
  // The requested baselines coincide exactly. Disable only extra D2D vertical
  // origin snapping; the native rasterizer's grid fitting remains enabled.
  t->DrawTextLayout(origin, app.nativeLayouts[index].Get(), brush, D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
  t->SetTextRenderingParams(previousParams.Get());
  t->SetTextAntialiasMode(previousAA);
}
bool panel(ID2D1RenderTarget* t, float top) {
  const auto dimensions = t->GetSize();
  const float width = (dimensions.width - 48) / 2;
  if (width < 50 || !rebuild(width - 28)) return false;
  const auto bg = D2D1::ColorF(app.dark ? 0x17191d : 0xffffff);
  const auto fg = D2D1::ColorF(app.dark ? 0xededed : 0x202124);
  ComPtr<ID2D1SolidColorBrush> brush;
  if (FAILED(t->CreateSolidColorBrush(fg, &brush))) return false;
  t->Clear(bg);
  const bool native = app.nativeMode != NativeMode::None;
  // Labels use grayscale in native diagnostics, independent of the samples.
  if (native) t->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
  label(t, brush.Get(), native ? L"同文件 Windows DirectWrite / LumaText · 基线对齐 / D2D NO_SNAP · 独立 shaping" :
    L"微软雅黑渲染对照 · 候选为实验方案，非原生 macOS", 16, top, dimensions.width - 32);
  wchar_t summary[256];
  swprintf_s(summary, L"字号 %.0f DIP · DPI %.0f · %s · gamma %.2f · 常规补偿 %.3f px · 粗体 %.3f px · 相位 %d/8 px · 候选 hint %s",
    app.size, app.dpi, filter_name(),
    app.gamma, app.regularWeight, app.boldWeight, app.phase, app.hinted ? L"开" : L"关");
  label(t, brush.Get(), summary, 16, top + 32, dimensions.width - 32, 42);
  auto fd = LumaText::Descriptor<lt_frame_desc>(); fd.dpi_x = fd.dpi_y = app.dpi;
  LumaText::Frame frame;
  if (lt_frame_begin(app.renderer.get(), &fd, frame.put()) != LT_OK) return false;
  bool ok = true;
  for (int side = 0; side < 2; ++side) {
    float x = 16 + static_cast<float>(side) * (width + 16);
    float y = top + 84;
    t->PushAxisAlignedClip(D2D1::RectF(x, y, x + width, dimensions.height - 12), D2D1_ANTIALIAS_MODE_ALIASED);
    std::wstring heading;
    if (native && side == 0) heading = std::wstring(L"DirectWrite · ") + native_name() + L" · 原生 gamma/contrast";
    else if (native) heading = std::wstring(L"LumaText · ") + (app.hinted ? L"FT hinted" : L"FT natural") + L" · " + filter_name() +
      (app.blend ? L" · 已知底色线性合成" : L" · 透明路径");
    else heading = side ? (app.blend ? L"候选 · 已知底色线性合成 + 光学补偿" : L"候选 · 透明路径 + 光学补偿") : L"当前 LumaText · 透明路径 / gamma 0.85";
    label(t, brush.Get(), heading, x + 8, y, width - 16, 44);
    y += 52;
    auto start = std::chrono::steady_clock::now();
    for (size_t i = 0; i < app.layouts.size(); ++i) {
      if (i == 0 || i == 4) {
        label(t, brush.Get(), i == 0 ? L"Regular · msyh.ttc face 0" : L"Bold · msyhbd.ttc face 0", x + 8, y, width - 16);
        y += 32;
      }
      auto d = LumaText::Descriptor<lt_draw_text_desc>();
      d.origin_x = x + 8;
      d.origin_y = y + static_cast<float>(app.phase) / 8 * 96 / app.dpi;
      d.foreground = {fg.r, fg.g, fg.b, 1}; d.background = {bg.r, bg.g, bg.b, 1};
      d.background_type = LT_BACKGROUND_TRANSPARENT;
      d.render_config = config(side == 1); d.profile = side ? app.profile.get() : nullptr;
      d.clip_enabled = true;
      d.clip = {x + 8, y - 2, x + width - 8, std::min(y + app.size * 1.65f, dimensions.height - 12)};
      if (d.clip.bottom > d.clip.top) {
        if (native && side == 0) {
          t->PushAxisAlignedClip(D2D1::RectF(d.clip.left, d.clip.top, d.clip.right, d.clip.bottom), D2D1_ANTIALIAS_MODE_ALIASED);
          draw_native(t, brush.Get(), i, D2D1::Point2F(d.origin_x, d.origin_y), app.nativeMode);
          t->PopAxisAlignedClip();
        } else if (lt_frame_draw_text_layout(frame.get(), app.layouts[i].get(), &d) != LT_OK) ok = false;
      }
      y += app.size * 1.65f + 8;
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    wchar_t timing[100]; swprintf_s(timing, L"本帧提交 %.2f ms（含缓存，非 GPU 耗时）", ms);
    label(t, brush.Get(), timing, x + 8, y + 8, width - 16, 42);
    t->PopAxisAlignedClip();
  }
  lt_frame_end(frame.get());
  return ok;
}
// These neutral strips are part of the exported PNG, so verification reads actual
// rendered RGB pixels rather than assuming that SetTextAntialiasMode took effect.
constexpr float probeY = 800, probeWidth = 600, probeHeight = 72;
constexpr std::array<float, 2> probeX{24, 736};
void native_probes(ID2D1RenderTarget* target) {
  ComPtr<ID2D1SolidColorBrush> black, white, caption;
  if (FAILED(target->CreateSolidColorBrush(D2D1::ColorF(0x000000), &black)) ||
      FAILED(target->CreateSolidColorBrush(D2D1::ColorF(0xffffff), &white)) ||
      FAILED(target->CreateSolidColorBrush(D2D1::ColorF(app.dark ? 0xededed : 0x202124), &caption))) return;
  for (size_t i = 0; i < probeX.size(); ++i) {
    label(target, caption.Get(), i ? L"RGB 自检 · DirectWrite ClearType RGB / black on white" :
      L"RGB 自检 · DirectWrite grayscale / black on white", probeX[i], probeY - 36, probeWidth);
    const auto rect = D2D1::RectF(probeX[i], probeY, probeX[i] + probeWidth, probeY + probeHeight);
    target->FillRectangle(rect, white.Get());
    target->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_ALIASED);
    draw_native(target, black.Get(), 2, D2D1::Point2F(probeX[i] + 8, probeY + 12),
      i ? NativeMode::ClearType : NativeMode::Grayscale);
    target->PopAxisAlignedClip();
  }
}
HWND control(HWND parent, const wchar_t* cls, const wchar_t* text, DWORD style, int id) {
  HWND h = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10,
    parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
  SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(app.controlFont), TRUE);
  if (id >= Theme && id <= Hint) app.controls[static_cast<size_t>(id - Theme)] = h;
  return h;
}
void update_control_font(HWND window) {
  HFONT font = CreateFontW(-MulDiv(14, static_cast<int>(GetDpiForWindow(window)), 96),
    0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
    CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
  if (!font) return;
  HFONT previous = app.controlFont;
  app.controlFont = font;
  for (HWND h : app.controls) if (h) SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
  for (HWND h : app.captions) if (h) SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
  if (previous) DeleteObject(previous);
}
HWND get(int id) { return app.controls[static_cast<size_t>(id - Theme)]; }
void choose(int id, int index) { SendMessageW(get(id), CB_SETCURSEL, index, 0); }
int chosen(int id) { return static_cast<int>(SendMessageW(get(id), CB_GETCURSEL, 0, 0)); }
void defaults() {
  app.updating = true;
  app.dark = false; app.size = 16; app.dpi = 96; app.gamma = .9f;
  app.regularWeight = .02f; app.boldWeight = 0; app.phase = 0; app.filter = 3;
  app.hinted = false; SendMessageW(get(Hint), BM_SETCHECK, BST_UNCHECKED, 0);
  app.blend = true; app.scroll = 0; SendMessageW(get(Blend), BM_SETCHECK, BST_CHECKED, 0);
  choose(Theme, 0); choose(Size, 2); choose(Dpi, 0); choose(Filter, 2); choose(Phase, 0);
  SetWindowTextW(get(Gamma), L"0.90"); SetWindowTextW(get(Regular), L"0.02"); SetWindowTextW(get(Bold), L"0.00");
  SetWindowTextW(get(Text), sample); app.text = sample; app.dirty = true; app.updating = false;
}
void arrange(HWND window) {
  RECT r; GetClientRect(window, &r);
  float scale = static_cast<float>(GetDpiForWindow(window)) / 96;
  auto move = [scale](HWND h, int x, int y, int w, int height) {
    MoveWindow(h, static_cast<int>(x * scale), static_cast<int>(y * scale), static_cast<int>(w * scale), static_cast<int>(height * scale), TRUE);
  };
  const int positions[] = {16, 140, 264, 388};
  for (int i = 0; i < 8; ++i) {
    int row = i / 4 * 52;
    move(app.captions[static_cast<size_t>(i)], positions[i % 4], 8 + row, 115, 20);
    move(get(Theme + i), positions[i % 4], 30 + row, 112, i < 4 || i == 7 ? 250 : 25);
  }
  move(get(Reset), 525, 29, 105, 28);
  move(get(Blend), 525, 80, 210, 28);
  move(get(Hint), 525, 57, 210, 23);
  move(get(Text), 16, 120, std::max(80, static_cast<int>(r.right / scale) - 32), 30);
}
void controls(HWND window) {
  update_control_font(window);
  const wchar_t* captions[] = {L"主题", L"字号 DIP", L"模拟 DPI", L"共同滤波器", L"候选 gamma", L"常规补偿 px", L"粗体补偿 px", L"基线相位"};
  const wchar_t* entries[] = {L"浅色|深色", L"12|14|16|20|24", L"96|120|144|192", L"direct|box|Mitchell", L"", L"", L"", L"0|1|2|3|4|5|6|7"};
  for (int i = 0; i < 8; ++i) {
    app.captions.push_back(control(window, L"STATIC", captions[i], 0, 0));
    bool combo = i < 4 || i == 7;
    HWND h = control(window, combo ? L"COMBOBOX" : L"EDIT", L"", combo ? CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP : WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, Theme + i);
    std::wstring options = entries[i]; size_t pos = 0;
    while (pos < options.size()) {
      size_t end = options.find(L'|', pos);
      std::wstring item = options.substr(pos, end - pos);
      SendMessageW(h, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.c_str()));
      if (end == std::wstring::npos) break; pos = end + 1;
    }
  }
  control(window, L"BUTTON", L"恢复默认", BS_PUSHBUTTON | WS_TABSTOP, Reset);
  control(window, L"BUTTON", L"统一背景合成", BS_AUTOCHECKBOX | WS_TABSTOP, Blend);
  control(window, L"BUTTON", L"候选：像素网格 hint", BS_AUTOCHECKBOX | WS_TABSTOP, Hint);
  control(window, L"EDIT", sample, WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, Text);
  defaults(); arrange(window);
}
float number(int id, float fallback, float lo, float hi) {
  wchar_t s[80]; GetWindowTextW(get(id), s, 80); wchar_t* end = nullptr;
  float value = wcstof(s, &end);
  return end != s && *end == 0 && std::isfinite(value) && value >= lo && value <= hi ? value : fallback;
}
void changed(HWND window, int id) {
  if (app.updating) return;
  if (id == Reset) defaults();
  else {
    app.dark = chosen(Theme) == 1;
    app.hinted = SendMessageW(get(Hint), BM_GETCHECK, 0, 0) == BST_CHECKED;
    app.blend = SendMessageW(get(Blend), BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (id == Theme) {
      app.updating = true; SetWindowTextW(get(Gamma), app.dark ? L"1.00" : L"0.90"); app.updating = false;
    }
    constexpr float sizes[] = {12,14,16,20,24}, dpis[] = {96,120,144,192};
    app.size = sizes[std::clamp(chosen(Size), 0, 4)]; app.dpi = dpis[std::clamp(chosen(Dpi), 0, 3)];
    app.filter = chosen(Filter) + 1; app.phase = std::max(0, chosen(Phase));
    app.gamma = number(Gamma, app.gamma, .1f, 3); app.regularWeight = number(Regular, app.regularWeight, 0, .5f);
    app.boldWeight = number(Bold, app.boldWeight, 0, .5f);
    int length = GetWindowTextLengthW(get(Text)); std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(get(Text), text.data(), length + 1); text.resize(static_cast<size_t>(length)); app.text = std::move(text);
    app.dirty = true;
  }
  InvalidateRect(window, nullptr, FALSE);
}
LRESULT CALLBACK procedure(HWND w, UINT m, WPARAM wp, LPARAM lp) {
  switch (m) {
    case WM_CREATE: controls(w); return 0;
    case WM_COMMAND:
      if (HIWORD(wp) == CBN_SELCHANGE || HIWORD(wp) == EN_CHANGE || LOWORD(wp) == Reset || LOWORD(wp) == Blend || LOWORD(wp) == Hint) changed(w, LOWORD(wp));
      return 0;
    case WM_SIZE:
      if (app.windowTarget) app.windowTarget->Resize(D2D1::SizeU(LOWORD(lp), HIWORD(lp)));
      if (!app.captions.empty()) arrange(w); InvalidateRect(w, nullptr, FALSE); return 0;
    case WM_DPICHANGED: {
      auto r = reinterpret_cast<RECT*>(lp); SetWindowPos(w, nullptr, r->left, r->top, r->right-r->left, r->bottom-r->top, SWP_NOZORDER);
      update_control_font(w); arrange(w); return 0;
    }
    case WM_GETMINMAXINFO: {
      auto info = reinterpret_cast<MINMAXINFO*>(lp); float s = static_cast<float>(GetDpiForWindow(w)) / 96;
      info->ptMinTrackSize = {static_cast<LONG>(780*s), static_cast<LONG>(550*s)}; return 0;
    }
    case WM_MOUSEWHEEL:
      app.scroll = std::max(0, app.scroll - static_cast<short>(HIWORD(wp)) / WHEEL_DELTA * 40);
      InvalidateRect(w, nullptr, FALSE); return 0;
    case WM_VSCROLL: {
      SCROLLINFO si{sizeof(si), SIF_ALL}; GetScrollInfo(w, SB_VERT, &si);
      int delta = LOWORD(wp) == SB_LINEUP ? -30 : LOWORD(wp) == SB_LINEDOWN ? 30 : LOWORD(wp) == SB_PAGEUP ? -150 : LOWORD(wp) == SB_PAGEDOWN ? 150 : 0;
      app.scroll = LOWORD(wp) == SB_THUMBTRACK ? si.nTrackPos : std::max(0, app.scroll + delta);
      InvalidateRect(w, nullptr, FALSE); return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
      PAINTSTRUCT ps; BeginPaint(w, &ps);
      if (!app.windowTarget) {
        RECT r; GetClientRect(w, &r);
        if (SUCCEEDED(app.factory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(), D2D1::HwndRenderTargetProperties(w, D2D1::SizeU(r.right, r.bottom)), &app.windowTarget))) attach(app.windowTarget.Get());
      }
      if (app.windowTarget && app.renderer) {
        app.windowTarget->SetDpi(app.dpi, app.dpi); app.windowTarget->BeginDraw();
        app.windowTarget->Clear(D2D1::ColorF(app.dark ? 0x17191d : 0xffffff));
        float top = 164.0f * static_cast<float>(GetDpiForWindow(w)) / app.dpi;
        auto dims = app.windowTarget->GetSize();
        int extent = static_cast<int>(top + 84 + 52 + 64 + 8 * (app.size * 1.65f + 8) + 70);
        int maximum = std::max(0, extent - static_cast<int>(dims.height));
        app.scroll = std::clamp(app.scroll, 0, maximum);
        SCROLLINFO si{sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS, 0, extent, static_cast<UINT>(dims.height), app.scroll, 0};
        SetScrollInfo(w, SB_VERT, &si, TRUE);
        app.windowTarget->PushAxisAlignedClip(D2D1::RectF(0, top, dims.width, dims.height), D2D1_ANTIALIAS_MODE_ALIASED);
        panel(app.windowTarget.Get(), top - static_cast<float>(app.scroll));
        app.windowTarget->PopAxisAlignedClip();
        if (app.windowTarget->EndDraw() == D2DERR_RECREATE_TARGET) { app.renderer.reset(); app.windowTarget.Reset(); }
      }
      EndPaint(w, &ps); return 0;
    }
    case WM_DESTROY: PostQuitMessage(0); return 0;
  }
  return DefWindowProcW(w, m, wp, lp);
}
struct ProbeStats {
  std::array<uint64_t, 2> ink{}, chromatic{};
  uint64_t differentRgb = 0;
  bool valid = false;
};
ProbeStats inspect_native_probes(IWICBitmap* bitmap) {
  ProbeStats stats;
  const float scale = app.dpi / 96;
  const UINT width = static_cast<UINT>(probeWidth * scale);
  const UINT height = static_cast<UINT>(probeHeight * scale);
  const UINT stride = width * 4;
  std::array<std::vector<BYTE>, 2> pixels;
  for (size_t i = 0; i < pixels.size(); ++i) {
    pixels[i].resize(static_cast<size_t>(stride) * height);
    WICRect rect{static_cast<INT>(probeX[i] * scale), static_cast<INT>(probeY * scale),
      static_cast<INT>(width), static_cast<INT>(height)};
    if (FAILED(bitmap->CopyPixels(&rect, stride, static_cast<UINT>(pixels[i].size()), pixels[i].data()))) return stats;
    for (size_t p = 0; p < pixels[i].size(); p += 4) {
      const auto* rgb = pixels[i].data() + p;
      if (rgb[0] != 255 || rgb[1] != 255 || rgb[2] != 255) ++stats.ink[i];
      if (rgb[0] != rgb[1] || rgb[1] != rgb[2]) ++stats.chromatic[i];
    }
  }
  for (size_t p = 0; p < pixels[0].size(); p += 4) {
    if (pixels[0][p] != pixels[1][p] || pixels[0][p+1] != pixels[1][p+1] || pixels[0][p+2] != pixels[1][p+2]) ++stats.differentRgb;
  }
  stats.valid = stats.ink[0] > 0 && stats.ink[1] > 0 && stats.chromatic[0] == 0 &&
    stats.chromatic[1] > 0 && stats.differentRgb > 0;
  return stats;
}
std::string json_string(const std::wstring& input) {
  const int size = WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), nullptr, 0, nullptr, nullptr);
  std::string utf8(static_cast<size_t>(size), '\0');
  if (size) WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), utf8.data(), size, nullptr, nullptr);
  std::string result = "\"";
  for (unsigned char c : utf8) {
    if (c == '\\' || c == '"') result += '\\';
    if (c < 32) { char escaped[7]; sprintf_s(escaped, "\\u%04x", c); result += escaped; }
    else result += static_cast<char>(c);
  }
  return result + '"';
}
bool write_native_metadata(const wchar_t* path, ID2D1RenderTarget* target, const ProbeStats& stats) {
  float dpiX = 0, dpiY = 0; target->GetDpi(&dpiX, &dpiY);
  D2D1_MATRIX_3X2_F transform{}; target->GetTransform(&transform);
  std::ostringstream out;
  out << "{\n  \"native_mode\": " << json_string(native_name())
      << ",\n  \"dpi\": [" << dpiX << ", " << dpiY << "], \"size_dip\": " << app.size
      << ", \"dark\": " << (app.dark ? "true" : "false") << ", \"phase_eighth_px\": " << app.phase
      << ",\n  \"target\": {\"type\": \"D2D software WIC 32bppBGR\", \"alpha_mode\": " << target->GetPixelFormat().alphaMode
      << ", \"alpha_ignore_expected\": " << D2D1_ALPHA_MODE_IGNORE
      << ", \"transform\": [" << transform._11 << ", " << transform._12 << ", " << transform._21 << ", "
      << transform._22 << ", " << transform._31 << ", " << transform._32 << "]},"
      << "\n  \"native\": {\"text_aa\": " << (app.nativeMode == NativeMode::ClearType ? D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE : D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE)
      << ", \"gamma\": " << app.nativeParams->GetGamma() << ", \"enhanced_contrast\": " << app.nativeParams->GetEnhancedContrast()
      << ", \"cleartype_level\": " << app.nativeParams->GetClearTypeLevel() << ", \"pixel_geometry\": " << app.nativeParams->GetPixelGeometry()
      << ", \"rendering_mode\": " << app.nativeParams->GetRenderingMode()
      << ", \"draw_options\": \"NO_SNAP; shared baseline; native grid fitting retained\", \"shaping\": \"DirectWrite, natural metrics, no wrap, no ellipsis\"},"
      << "\n  \"lumatext\": {\"filter\": " << json_string(filter_name()) << ", \"hinted\": " << (app.hinted ? "true" : "false")
      << ", \"base_coverage_gamma\": " << app.gamma << ", \"regular_optical_weight_px\": " << app.regularWeight
      << ", \"bold_optical_weight_px\": " << app.boldWeight << ", \"known_background_linear_blend\": " << (app.blend ? "true" : "false")
      << ", \"render_flags\": " << config(true).flags << ", \"background_type\": \"LT_BACKGROUND_TRANSPARENT\"},"
      << "\n  \"fonts\": [";
  for (size_t i = 0; i < app.fontPaths.size(); ++i) {
    if (i) out << ", ";
    out << "{\"path\": " << json_string(app.fontPaths[i]) << ", \"face_index\": 0, \"weight\": " << (i ? 700 : 400)
        << ", \"family\": " << json_string(app.nativeFamilies[i]) << ", \"family_index\": " << app.nativeFamilyIndices[i]
        << ", \"all_native_runs_exact_file_face_verified\": true}";
  }
  out << "],\n  \"line_baselines\": [";
  for (size_t i = 0; i < app.layouts.size(); ++i) {
    if (i) out << ", ";
    out << "{\"luma_ascent_dip\": " << app.lumaAscent[i] << ", \"native_origin_shift_dip\": " << app.nativeBaselineShift[i] << "}";
  }
  out << "],\n  \"rgb_probe\": {\"passed\": " << (stats.valid ? "true" : "false")
      << ", \"gray_ink_pixels\": " << stats.ink[0] << ", \"cleartype_ink_pixels\": " << stats.ink[1]
      << ", \"gray_chromatic_pixels\": " << stats.chromatic[0] << ", \"cleartype_chromatic_pixels\": " << stats.chromatic[1]
      << ", \"different_rgb_pixels\": " << stats.differentRgb
      << ", \"rects_dip\": [[24, 800, 600, 72], [736, 800, 600, 72]]},"
      << "\n  \"limitations\": \"Controlled DWrite reference, not every Windows application's settings; independent shapers; DWrite gamma and LumaText coverage gamma are not equivalent parameters.\"\n}\n";
  FILE* file = nullptr;
  const std::wstring metadata = std::wstring(path) + L".json";
  if (_wfopen_s(&file, metadata.c_str(), L"wb") != 0 || !file) return false;
  const auto bytes = out.str();
  const bool written = fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
  return fclose(file) == 0 && written;
}
bool snapshot(const wchar_t* path) {
  ComPtr<IWICImagingFactory> wic; ComPtr<IWICBitmap> bitmap; ComPtr<ID2D1RenderTarget> target;
  const bool native = app.nativeMode != NativeMode::None;
  const UINT pixelWidth = static_cast<UINT>(1440 * app.dpi / 96);
  const UINT pixelHeight = static_cast<UINT>(900 * app.dpi / 96);
  if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic))) ||
      FAILED(wic->CreateBitmap(pixelWidth, pixelHeight, native ? GUID_WICPixelFormat32bppBGR : GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &bitmap)) ||
      FAILED(app.factory->CreateWicBitmapRenderTarget(bitmap.Get(), D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, native ? D2D1_ALPHA_MODE_IGNORE : D2D1_ALPHA_MODE_PREMULTIPLIED), app.dpi, app.dpi), &target)) || !attach(target.Get())) return false;
  target->BeginDraw(); bool ok = panel(target.Get(), 16);
  if (ok && native) native_probes(target.Get());
  HRESULT hr = target->EndDraw();
  if (!ok || FAILED(hr)) return false;
  if (native) {
    const auto stats = inspect_native_probes(bitmap.Get());
    if (!write_native_metadata(path, target.Get(), stats) || !stats.valid) return false;
  }
  ComPtr<IWICStream> stream; ComPtr<IWICBitmapEncoder> encoder; ComPtr<IWICBitmapFrameEncode> frame;
  WICPixelFormatGUID format = native ? GUID_WICPixelFormat24bppBGR : GUID_WICPixelFormat32bppBGRA;
  return SUCCEEDED(wic->CreateStream(&stream)) && SUCCEEDED(stream->InitializeFromFilename(path, GENERIC_WRITE)) &&
    SUCCEEDED(wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) && SUCCEEDED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)) &&
    SUCCEEDED(encoder->CreateNewFrame(&frame, nullptr)) && SUCCEEDED(frame->Initialize(nullptr)) && SUCCEEDED(frame->SetSize(pixelWidth, pixelHeight)) &&
    SUCCEEDED(frame->SetResolution(app.dpi, app.dpi)) && SUCCEEDED(frame->SetPixelFormat(&format)) && SUCCEEDED(frame->WriteSource(bitmap.Get(), nullptr)) &&
    SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());
}
bool initialize() {
  if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, app.factory.GetAddressOf())) ||
      FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(app.write.GetAddressOf())))) return false;
  auto c = LumaText::Descriptor<lt_context_desc>(); c.dwrite_factory = app.write.Get(); c.cpu_cache_limit_bytes = 32ull*1024*1024;
  if (lt_context_create(&c, app.context.put()) != LT_OK) return false;
  wchar_t windows[MAX_PATH]; if (!GetWindowsDirectoryW(windows, MAX_PATH)) return false;
  auto face = [&](const wchar_t* name, size_t index, LumaText::FontFace& out) {
    std::wstring path = std::wstring(windows) + L"\\Fonts\\" + name;
    app.fontPaths[index] = path;
    auto f = LumaText::Descriptor<lt_font_source_desc>(); f.source_type = LT_FONT_SOURCE_FILE; f.file_path = path.c_str();
    return lt_font_face_create(app.context.get(), &f, out.put()) == LT_OK;
  };
  if (!face(L"msyh.ttc", 0, app.regular) || !face(L"msyhbd.ttc", 1, app.bold)) return false;
  lt_font_cascade_entry entries[] = {{app.regular.get(),400,0},{app.bold.get(),700,0}};
  auto cd = LumaText::Descriptor<lt_font_cascade_desc>(); cd.entries = entries; cd.entry_count = 2; cd.allow_system_fallback = false;
  return lt_font_cascade_create(app.context.get(), &cd, app.cascade.put()) == LT_OK &&
    SUCCEEDED(app.write->CreateTextFormat(L"Microsoft YaHei", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
      DWRITE_FONT_STRETCH_NORMAL, 13, L"zh-CN", &app.label)) &&
    (app.nativeMode == NativeMode::None || initialize_native());
}
} // namespace
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 1;
  int argc = 0; LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc); std::wstring output;
  for (int i = 1; argv && i < argc; ++i) {
    if (wcscmp(argv[i], L"--snapshot") == 0 && i + 1 < argc) output = argv[++i];
    else if (wcscmp(argv[i], L"--dark") == 0) { app.dark = true; app.gamma = 1; }
    else if (wcscmp(argv[i], L"--hinted") == 0) app.hinted = true;
    else if (wcscmp(argv[i], L"--native") == 0) {
      if (i + 1 >= argc) { LocalFree(argv); return 1; }
      const wchar_t* mode = argv[++i];
      if (wcscmp(mode, L"grayscale") == 0) app.nativeMode = NativeMode::Grayscale;
      else if (wcscmp(mode, L"cleartype") == 0) app.nativeMode = NativeMode::ClearType;
      else { LocalFree(argv); return 1; }
    }
    else if (wcscmp(argv[i], L"--isolate-hint") == 0) {
      app.gamma = .85f; app.regularWeight = app.boldWeight = 0; app.blend = false;
    }
    else if (wcscmp(argv[i], L"--filter") == 0 && i + 1 < argc) {
      const wchar_t* filter = argv[++i];
      if (wcscmp(filter, L"direct") == 0) app.filter = LT_RASTER_FILTER_DIRECT;
      else if (wcscmp(filter, L"box") == 0) app.filter = LT_RASTER_FILTER_BOX;
      else if (wcscmp(filter, L"mitchell") == 0) app.filter = LT_RASTER_FILTER_MITCHELL;
      else { LocalFree(argv); return 1; }
    }
    else if (wcscmp(argv[i], L"--no-blend") == 0) app.blend = false;
    else if (wcscmp(argv[i], L"--known-background") == 0) app.blend = true;
    else if (wcscmp(argv[i], L"--phase") == 0 && i + 1 < argc) app.phase = std::clamp(_wtoi(argv[++i]), 0, 7);
    else if (wcscmp(argv[i], L"--dpi") == 0 && i + 1 < argc) app.dpi = static_cast<float>(std::clamp(_wtoi(argv[++i]), 96, 192));
    else if (wcscmp(argv[i], L"--size") == 0 && i + 1 < argc) app.size = static_cast<float>(std::clamp(_wtoi(argv[++i]), 12, 24));
  }
  if (argv) LocalFree(argv);
  if (app.nativeMode != NativeMode::None && output.empty()) return 1; // Native references are snapshot-only.
  if (!initialize()) {
    if (output.empty()) MessageBoxW(nullptr, L"初始化失败。需要 Windows 微软雅黑 msyh.ttc 与 msyhbd.ttc。", L"LumaText 对照", MB_ICONERROR);
    return 2;
  }
  if (!output.empty()) return snapshot(output.c_str()) ? 0 : 3;
  WNDCLASSW wc{}; wc.lpfnWndProc = procedure; wc.hInstance = instance; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.lpszClassName = L"LumaTextCompare";
  RegisterClassW(&wc);
  const UINT windowDpi = GetDpiForSystem();
  RECT workArea{};
  if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0)) {
    workArea.right = GetSystemMetrics(SM_CXSCREEN); workArea.bottom = GetSystemMetrics(SM_CYSCREEN);
  }
  const int windowWidth = std::min(MulDiv(1200, static_cast<int>(windowDpi), 96), static_cast<int>(workArea.right - workArea.left));
  const int windowHeight = std::min(MulDiv(850, static_cast<int>(windowDpi), 96), static_cast<int>(workArea.bottom - workArea.top));
  HWND window = CreateWindowExW(0, wc.lpszClassName, L"LumaText · 微软雅黑候选对照", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VSCROLL,
    workArea.left + (workArea.right - workArea.left - windowWidth) / 2,
    workArea.top + (workArea.bottom - workArea.top - windowHeight) / 2,
    windowWidth, windowHeight, nullptr, nullptr, instance, nullptr);
  if (!window) return 4;
  ShowWindow(window, show); MSG msg{};
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) { if (!IsDialogMessageW(window, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); } }
  if (app.controlFont) { DeleteObject(app.controlFont); app.controlFont = nullptr; }
  return static_cast<int>(msg.wParam);
}
