# 微软雅黑实时对照

开启 `LUMATEXT_BUILD_SAMPLES=ON` 后构建 `lumatext_compare`。当前可运行文件：

`out/build-sdk/samples/compare/Release/lumatext_compare.exe`

与旁边的 `lumatext.dll` 一起使用。两栏均使用 Windows 的 `msyh.ttc` 与 `msyhbd.ttc`，没有用其他字体替换半粗体，也没有把模拟结果当作原生 macOS。

## 操作

- 左栏：当前透明背景绘制方式，基础 gamma 0.85。
- 右栏：实验候选，可调整 gamma、常规/粗体光学补偿，并独立开启或关闭“统一背景合成”。浅色起点 gamma 0.90，深色起点 1.00，常规补偿 0.02 px、粗体 0；它们是比较起点，不是已签收的 macOS 校准值。
- 共同控件：字号、模拟 DPI、Direct/Box/Mitchell、基线 0–7/8 像素相位。共同滤波器同时作用两栏，以便单独观察合成和补偿差异。
- 编辑上方文字可替换首行；下方保留中文、英文数字和常规/粗体样例。
- 大字号或高模拟 DPI 下，可滚动查看后面的样例；“恢复默认”恢复所有实验选项。
- 提交耗时包括当前调用和缓存，不是 GPU 耗时。切换参数后的首帧通常比重复绘制慢。

候选合成可能让浅底文字更轻、深底文字更亮。请分别判断两种主题，不要只看哪边更黑或更锐。macOS 同字体参考图尚未采集，当前不能据此宣称复现了 CoreText。

## 后端变化

1. 新增可选 `LT_RENDER_CONFIG_KNOWN_BACKGROUND`：单行透明绘制时，调用者明确提供真实、均匀、不透明的背景（`background.a == 1`），使用与实色背景一致的线性合成。它会预合成背景区域，不能用于未知底色、图片、渐变或透明材质；不启用时保持原透明路径。
2. `regular_optical_weight` / `bold_optical_weight` 接入轮廓补偿，单位为物理像素，范围 0–1。独立于 gamma，不改变 advance；真实粗体也支持显式补偿。`DISABLE_STEM_COMPENSATION` 同时禁用这项补偿。字形和整行缓存均包含相应参数。
3. 修正垂直相位方向：屏幕坐标向下、FreeType 向上。新增三种滤波器的 8 相位重心扫描，修复前失败、修复后通过；非零垂直相位输出会因此改变，两栏均使用修复后的定位。
4. 旧 profile descriptor 没有光学补偿尾字段时默认取 0，不再读取声明范围外的尾字段。

尚未实施按脚本自动选参、自动选择滤波器或扩大至 1/16 相位；这些应在拿到同字体 CoreText 参考与实屏反馈后决定，而不是在对照前改掉默认观感。

## 离屏图片

快照与窗口使用同一面板绘制函数和实际 LumaText 渲染器：

```powershell
lumatext_compare.exe --snapshot light.png
lumatext_compare.exe --snapshot dark.png --dark --dpi 144 --size 14
lumatext_compare.exe --snapshot phase.png --phase 7 --no-blend
```

已有示例在 `out/compare/light.png` 与 `out/compare/dark.png`。截图命令不包含原生工具栏；界面中仍可实时调参。

## 验证

Debug/Release 各 10 项测试通过，包含已知背景与实色输出逐像素一致、非法半透明底色拒绝、光学补偿缓存失效及真实粗体补偿、旧 profile 尾字段兼容、垂直相位方向回归。已使用原生窗口检查主题切换与参数编辑，并检查浅色/深色离屏图片。

## 低 DPI hint 候选（2026-09-30）

新增“候选：像素网格 hint”复选框，只作用于右栏，默认关闭。Box/Mitchell
现在先在实际目标像素网格 hint，再放大轮廓超采样；默认未 hint 管线不变。
快照新增 `--hinted` 和 `--filter direct|box|mitchell`。恢复默认会关闭 hint。
这不是已签收的视觉参数；测试步骤与本轮实际验证范围见
[低 DPI 验证说明](low-dpi-validation.md)。
