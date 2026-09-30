# 字体与整行缓存优化

日期：2026-09-20。接续首轮渲染审查，保留原有灰度参数和渲染算法。

## 已实现

### 字体请求

`lt_glyph_request` 尾部新增可选 `font_face`。调用方通过已有 `lt_font_face_create` 创建一次字体，再重复传入该句柄；请求直接使用已加载字体，不再对整份 font_bytes 做哈希。新字段优先于 DirectWrite 和原始字节来源，句柄必须来自同一 context，并在调用期间有效。

旧请求大小仍兼容，不足一个完整指针的尾部不会读取。旧 font_bytes 调用保持原有内容识别行为，不按裸指针缓存，以免源缓冲区修改后错误复用。使用新头文件和 DLL，并切换到 font_face，才能获得这一项收益。详见 [SDK 接入说明](shared-sdk.md)。

### 重复绘制

普通单行文本的完整合成位图按 renderer 做 LRU 缓存，最多 64 项、8 MiB 像素数据（不包含驱动内部开销）。命中时直接绘制位图，跳过逐字查缓存、CPU 合成和位图创建/上传。

缓存键包含不可复用的 layout 身份、精确 origin、DPI、前景/背景颜色、背景类型和生效的渲染参数。裁剪在绘制时执行，不写入缓存图片；彩色字体 run 暂不进入整行缓存。缓存不持有 layout 或 cascade，布局释放后不会因此继续保留字体。

DPI 变化、替换 target、检测到设备丢失时清空缓存。补齐既有 `lt_d2d_renderer_set_target` 的公共声明和 DLL 导出；该调用须在 owner 线程且没有活动 frame 时执行。新增 `lt_frame_stats.line_cache_hits` 统计命中，旧 stats 前缀仍支持。

## 测量

Release，Microsoft YaHei，同机 WIC 软件渲染目标，12 行混合中英文、150% DPI、120 帧，绘制位置和 layout 保持不变。保留修改前静态链接的基准程序，与修改后程序交替运行三组。以下为三组中位数，包含首次冷绘制：

| 重绘场景 | 修改前总耗时 | 修改后总耗时 | 耗时减少 |
|---|---:|---:|---:|
| 透明背景 | 625.775 ms | 63.574 ms | 89.8% |
| 实色背景 | 9251.913 ms | 121.877 ms | 98.7% |

预加载字体接口使用同一 19,704,352 字节字体、相同四个 glyph，先预热再请求 100 次，三组中位数：原始字节入口约 **24.384 ms/次**，font_face 入口约 **0.249 μs/次**。这主要消除了大型字体的全文哈希；不包含字体创建和冷栅格化成本，极短调用的测量容易受计时和调度波动影响。

原始记录：`out/render-audit/cache-benchmark-runs.txt`。复现程序：

```powershell
out/build-sdk/tests/Release/lumatext_line_cache_test.exe --benchmark
out/build-sdk/tests/Release/lumatext_glyph_provider_benchmark.exe
```

这些是固定内容重绘和热字形请求基准，不是实际应用 FPS。文字、位置频繁变化时整行命中率会降低；整像素滚动也会产生不同 origin，目前没有复用平移后的行图像。字体和布局本身也应由宿主复用。

## 验证与产物

- Debug、Release 各 10 项测试全部通过。
- 缓存回归逐像素对比首次/再次绘制与全新 renderer，覆盖前景和背景色、透明度、背景模式、亚像素位置、gamma、滤波器、补偿、profile、裁剪、DPI 和 target 变化。
- 验证旧 stats 前缀、布局释放、不保留 context、容量限制及淘汰后重新绘制。
- 字体接口验证旧 descriptor 保护页、新尾部不完整指针、不同 context 拒绝、来源优先级、源字节释放后的使用、字形像素与 metrics 一致性。
- 现有 reference 的 100 张 PNG 与首轮修复后版本哈希完全一致。
- 已重新生成 `out/sdk/Debug` 和 `out/sdk/Release`，验证 DLL 不依赖 ICU；Release DLL 为 1,621,504 字节，Debug 为 6,635,520 字节。新接口导出已检查。

未模拟硬件设备丢失，未运行真实宿主的 GPU/滚动验收。线程局部 FreeType 字体表的容量策略及旧 DirectWrite 兼容路径的整 run 预检仍为后续工作。
