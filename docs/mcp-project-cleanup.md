# LumaText 保守清理记录

日期：2026-09-21。

## 授权范围与保护原则

用户选择“保守清理”：只清理临时文件、无用日志和废弃构建产物，保留源码、测试、必要文档、现有 SDK 和依赖缓存。本次不进行代码精简、依赖升级、功能调整或项目结构重组。

开始时仓库已经有多项修改、未跟踪源码及 `plan.md` 的既有删除状态，均予以保留；没有执行 Git reset、clean、add 或 commit。

## 清理结果

共删除 **86 个可再生成文件，合计 17,515,960 bytes（约 16.70 MiB）**，无删除失败。

| 类型 | 文件数 | 字节数 |
|---|---:|---:|
| 旧构建的本项目 `.obj` 中间文件 | 82 | 17,451,158 |
| 旧 `CMakeConfigureLog.yaml` 配置日志 | 3 | 50,933 |
| Python 字节码缓存 | 1 | 13,869 |
| 合计 | 86 | 17,515,960 |

| 所在目录 | 文件数 | 字节数 |
|---|---:|---:|
| `build-min-nmake/` | 32 | 6,506,830 |
| `build-release/` | 18 | 2,107,670 |
| `build-vs18/` | 35 | 8,887,591 |
| `tools/__pycache__/` | 1 | 13,869 |

旧构建清理项均超过 7 天；不是按整个目录删除。所有目标在删除前检查了文件类型/作用、Git 跟踪状态、路径边界、重解析点、硬链接数量、大小、修改时间及 SHA-256。检测到没有构建工具进程后，按冻结清单逐文件删除，没有递归清空目录。

保留必要的审计清单、压缩基线和本文后，项目文件逻辑体积净减少约 **16.2 MiB**。这是文件大小估算，不是对 NTFS 实际可用空间增量的精确测量。

## 明确保留

- 全部源码、公开头文件、测试、示例、必要脚本、既有文档及许可证。
- `out/build-sdk/` 当前 SDK 构建目录及增量构建状态。
- `out/sdk/`、`out/sdk-combined/`、所有既有 `out/install*/` SDK/安装树。
- 所有 `_deps/` 依赖源码、下载和构建缓存，以及 HarfBuzz 对象缓存。
- `build-min-nmake/_deps/freetype-src/` 与 `build-min-nmake/_deps/harfbuzz-src/`：`build-sdk.bat` 和当前 SDK CMake 配置仍在引用它们，不能整体删除 `build-min-nmake/`。
- 旧构建目录中的 DLL、LIB、EXE、PDB、CMake 配置和已有测试结果。
- Python 虚拟环境、字体、CI 对照数据、渲染审计、参数搜索结果和参考图。
- Git 暂存区内容及用户清理前已经存在的修改/删除状态。

因此没有为了扩大释放空间而删除占用较大的依赖、字体、虚拟环境或对照资料。以后再次构建旧的三个配置时，被清理的对象文件会重新编译；当前 `out/build-sdk/` 未被清理。

## 防止缓存再次污染工作区

仅为 `.gitignore` 增加以下 Python 字节码规则，不修改业务代码：

```gitignore
__pycache__/
*.pyc
*.pyo
```

已用不存在的探测路径执行 `git check-ignore`，确认三条规则生效，没有为测试忽略规则再创建缓存。

## 校验

- 核对 385 个源码、配置及 SDK/安装树条目的哈希或缺失状态；除批准的 `.gitignore` 修改外，清理前受保护内容保持不变，原来缺失的文件也没有被恢复。
- 对保留文件的清单、大小、修改时间及重解析点清单进行前后比对，未发现意外变化；本次新增文档和审计目录明确单列。
- Git 暂存条目与清理前一致。
- Debug/Release SDK manifest 的 **17 项大小与 SHA-256 校验全部通过**，清理前后结果一致。
- `build-sdk.bat` 引用的两个依赖源目录入口仍然存在。
- `.gitignore` 的 `git diff --check` 通过。Git 提示未来可能按本地配置转换 LF/CRLF；本次未更改 Git 配置。
- 本次没有修改业务源码，因此没有重新编译、运行功能测试或重新打包 SDK；以上内容完整性检查不冒充功能/视觉验收。
- 没有安装、替换或启动已安装应用，也没有修改 Pulse 或 LumaShot。

## 可复核材料

- `out/cleanup-20260921/inventory.json`：分类与体积统计。
- `out/cleanup-20260921/candidates.json`：删除前的完整候选清单与校验值。
- `out/cleanup-20260921/baseline.json.gz`：压缩的保留文件基线、Git 状态与暂存条目。
- `out/cleanup-20260921/deletion-journal.jsonl`：逐文件删除记录。
- `out/cleanup-20260921/deletion-result.json`：删除结果。
- `out/cleanup-20260921/verification.json`：最终保留内容校验。
- `out/cleanup-20260921/conservative_cleanup.py`：本次使用的受限维护脚本，不是产品代码，也不会由正常构建自动运行。
