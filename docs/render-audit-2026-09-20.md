# 渲染与性能检查（2026-09-20）

本轮基于已有未提交改动继续修改，未回退原工作。保留默认 gamma=0.85、contrast=1.0、Mitchell；不据单台机器截图重新冻结视觉参数。

## 已修复

- Direct 栅格模式的 advance 原来始终除以 256，导致返回值缩小为四分之一；现在按实际栅格倍率换算。
- Direct 模式的 stem/synthetic 补偿原来仍乘 4，导致明显过粗；现在使用实际倍率。
- glyph provider 接受只到 px_em 的请求前缀，却继续读取 DPI、相位和配置。现在先按调用者长度复制并补默认值；新增保护页测试，直接覆盖真实越界风险。
- 内存字体 key 原来只包含 face_index 的低 16 位，导致高位不同的请求误用已有 face；补齐全部 32 位。超出 FreeType 长度范围的字体数据在读取前拒绝。
- glyph provider 的字体加载、缓存分配异常原来可能越过 C ABI；现在统一转为错误码。数值量化先限制范围再取整。
- 彩色 emoji 的 palette alpha 忽略 foreground.a；现在遵守文字透明度，并覆盖 0、0.5、1 的实际像素测试。
- fi 合字 cluster 被截到单个 grapheme，第二个字符命中错误；现在使用 shaped run 内真实 cluster 边界。
- LTR/RTL 文本末尾光标选错 cluster/边缘；现在选择逻辑末尾 cluster 的尾边。

## 性能优化与测量

滤波权重由逐像素重复计算改为按列/行预计算；coverage gamma/contrast 使用线程局部 256 项查找表。保持原浮点累加顺序和边缘归一化。

新增 `tests/rasterizer_benchmark.cpp`，在 Release 下直接调用 Rasterizer，绕过 glyph cache；Microsoft YaHei face 0，glyph ID 1–256，12–24 DIP，96/144 DPI，8×8 相位组合，重复三轮，每次共 768 个字形。每个版本启动 5 次，以下为耗时中位数：

| 模式 | 修改前 | 修改后 | 耗时变化 |
|---|---:|---:|---:|
| Direct | 9.728 ms | 9.537 ms | -2.0% |
| Box | 104.562 ms | 91.898 ms | -12.1% |
| Mitchell | 136.685 ms | 67.453 ms | -50.7% |

机器负载存在明显波动，尤其修改后；数字仅代表这组 CPU 栅格微基准，不代表滚动帧率、缓存命中耗时或 GPU 性能。Direct 的微小差异不宜视为确定收益。原始输出在 `out/render-audit/benchmark-before.txt` 和 `benchmark-after.txt`。

三种模式的像素 checksum 分别保持为 16808444749282366296、14289952689746502446、2362316908280287642。基准使用未加粗轮廓，因此 Direct 补偿修复另由回归测试检查。

## 渲染效果与验证

- 使用现有 Windows reference 工具，Microsoft YaHei regular/bold，gamma=0.85、contrast=1、stem=0、Mitchell。
- 覆盖 100%、125%、150%、200%、300% 缩放，浅/深背景，共 50 个渲染记录及对应 mask，共 100 张 PNG。修改前后 PNG 哈希全部相同。
- 抽查 100% 小字号英文、100% 深色中文、150% 浅色中文，未见新增缺字、裁切或边缘退化。深色小字号中文字重视觉上较实；是否进一步减重仍需实际屏幕与宿主应用 A/B，不在本轮凭截图调整。
- Debug、Release 全部 9 项测试通过；新增断言纳入既有 glyph_provider、rasterizer_bounds、color_glyph、coretext_layout 测试。
- 未进行真实宿主应用滚动、4K 屏幕实机验收、GPU 计时或跨 Windows 版本验证。

复现：构建 `out/build-sdk` 对应配置后运行 `ctest --test-dir out/build-sdk -C Release --output-on-failure`；性能基准位于 `out/build-sdk/tests/Release/lumatext_rasterizer_benchmark.exe`。

## 后续优先项（未在本轮实现）

1. 内存字体请求每次仍对整个 font_bytes 做哈希。即使命中字形缓存也会扫描字体文件；适合增加可复用字体句柄入口，避免每字扫描大型中文字库，不能仅按裸指针缓存而忽略内容变化。
2. 新单行绘制路径每次重新合成 CPU 像素并创建 D2D bitmap。应先用宿主长列表测量 composition/upload 占比，再决定复用位图或引入 atlas。
3. 线程局部 FreeType face 表和 context 内存字体表没有独立容量限制。长期动态切换字体/反复创建 context 时应测试保留内存并设计淘汰策略。
4. 旧 DirectWrite 兼容路径逐字绘制失败后回退整 run，可能重复覆盖此前成功字形；建议后续加入可注入失败的渲染回归后改为整 run 预检。

这些是代码路径检查发现的后续风险，未量化为当前应用的已发生故障。

后续已实现预加载字体请求入口和有界整行重绘缓存；实现边界、复现与数据见
[缓存优化记录](render-cache-optimization.md)。线程局部字体表的容量策略及旧兼容路径整 run 预检仍未在这轮修改。
