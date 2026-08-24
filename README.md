# LumaText

LumaText 是一个面向 Windows 10/11 x64 的 C++20 灰度字体渲染器。DirectWrite 继续负责 shaping、fallback、双向文字、换行、测量和命中测试；LumaText 从同一 `IDWriteTextLayout` 读取 glyph run，通过 DirectWrite font stream 取得字体数据，再由 FreeType 2.13.3 栅格化为灰度覆盖并使用 D2D A8 opacity mask 绘制。

当前版本是 `0.1.0` 阶段 0/1 原型。它用于 Pulse A/B 实机验收，不是方案中的完整 `1.0`：D3D11 atlas、异步冷缓存和单行 TSF 输入尚未实现，相应 API 会明确返回 `LT_E_UNSUPPORTED`。

## 已实现

- 稳定 C ABI 和头文件式 C++ RAII 包装，descriptor 支持尾字段扩展。
- DirectWrite 字体文件流桥接，支持单文件 TrueType/OpenType/TTC face 和 variable axes。
- 每线程独立 FreeType library/face，避免跨线程共享 FreeType 对象。
- 带内存上限的 CPU glyph LRU cache；key 包含 font face 身份、glyph、字号、DPI、子像素相位和校准参数。
- 自然灰度轮廓、可调 coverage gamma/contrast 和轻微水平方向字干补偿。
- D2D A8 `FillOpacityMask` 后端、裁剪、下划线/删除线、设备线程检查和 `D2DERR_RECREATE_TARGET` 映射。
- 彩色字体、RTL/sideways 和无法读取的 font run 自动回退 D2D/DirectWrite 绘制，不输出空白 glyph。
- DLL、静态库、安装包 targets、Win32 A/B 画廊、ABI 测试和离屏像素烟雾测试。

## 构建

在 x64 Visual Studio Developer PowerShell 中执行：

```powershell
cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

CMake 默认从带 SHA-256 校验的 FreeType `VER-2-13-3` 上游归档构建。也可以设置 `-DLUMATEXT_USE_SYSTEM_FREETYPE=ON` 使用系统包。

画廊位于 `build/samples/gallery/lumatext_gallery.exe`。按 `Space` 切换 A/B 视图，按 `D` 切换浅色/深色背景。

## 使用

```cmake
find_package(LumaText 0.1 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE LumaText::D2D)
```

可用 targets 为 `LumaText::Core`、`LumaText::D2D`、`LumaText::D3D11`、`LumaText::Input`、`LumaText::Shared` 和 `LumaText::Static`。`D3D11` 与 `Input` target 目前只保证接入端无需改 CMake；调用其创建 API 仍返回未实现。

最小 API 使用流程见 [samples/gallery/main.cpp](samples/gallery/main.cpp)。renderer 绑定创建它的设备线程。宿主可通过 `lt_d2d_desc.manage_begin_end_draw` 选择由 LumaText 或宿主管理 `BeginDraw/EndDraw`。

## 验收闸门

在继续 D3D11 和输入模块之前，必须按 [docs/phase-1-acceptance.md](docs/phase-1-acceptance.md) 在 4K/150% 实机对比签收并冻结默认校准参数。当前 macOS-like 灰度基线使用未 hint 的自然轮廓，默认值为 `gamma=0.43`、`contrast=1.92`、`stem_strength=0.00px`，并按 physical em 与字重执行小字号 optical gamma 校准；实际粗体不会再次增加字干宽度。这仍不代表已经通过视觉验收。

LumaText 自身使用 MIT 许可证。固定依赖 FreeType 使用 FTL；完整许可文本见 [LICENSES/FreeType.txt](LICENSES/FreeType.txt)。本项目不包含任何字体文件。
