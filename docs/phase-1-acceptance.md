# Phase 1 实机验收

本闸门用于决定是否继续阶段 2。没有完成并签收本页矩阵前，不得把 `0.1` 原型描述为生产渲染器。

## 构建与记录

1. 使用 Release x64 构建，记录 LumaText 提交、Windows build、GPU/驱动、显示器型号和缩放。
2. 运行 `lumatext_gallery.exe`，同屏保留 DirectWrite 原生与 LumaText 两列。
3. 禁用截图缩放与图片压缩，保存原始 PNG；每次只改变一个校准参数。
4. 对 Pulse 文件列表做真实 A/B，不用画廊结果代替宿主集成结果。
5. 使用 `tools/compare_reference.py` 生成 `comparison.json`；报告同时记录 SSIM、coverage 差异、边缘最大/平均 alpha、halo 像素数和横竖笔画宽度差。

## 必测矩阵

| 维度 | 值 |
|---|---|
| DIP | 12、13、14、16 |
| DPI | 100%、125%、150%、200%、300% |
| 背景 | 浅色、深色、高对比度、透明材质、已知纯色 |
| 内容 | 中文、英文、数字、标点、混合文件名、粗体、variable font、fallback |
| 设备 | 4K/150% 实机、硬件 D2D、WARP、跨显示器 DPI 切换 |

## 通过条件

- DirectWrite 的布局宽度、换行、基线和命中测试结果不变。
- 无 RGB 彩边、暗边、光晕、裁切或浅/深背景明显字重跳变。
- 冷缓存失败时整个 run 可见，由 DirectWrite 回退承接。
- 由用户在 4K/150% Pulse 文件列表中确认小字号中文笔画达到目标。
- 签收后记录并冻结 `coverage_gamma`、`coverage_contrast` 和 `stem_strength`。

当前候选值为 `gamma=0.85`、`contrast=1.00`、`stem=0.00px`、`Mitchell`。该值在 Gallery 的 DirectWrite 对照中降低了低覆盖边缘的增益，避免常规和粗体行出现明显偏粗；PingFang 跨平台离屏矩阵仍需在同字体版本和 Microsoft YaHei UI / Pulse 4K/150% 实机上重新签收，因此不能视为最终冻结值。

## 当前非闸门项

D3D11 5,000 glyph/p95 1.5 ms、16 MiB atlas、异步冷缓存、设备丢失压力测试和 TSF/IME/UIA 属于后续阶段，当前 `0.1` 不声称满足这些标准。
