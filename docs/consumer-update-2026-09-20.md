# 四个使用项目更新记录（2026-09-20）

来源为当前 LumaText 工作区生成的 x64 SDK，包含未提交修改，不代表远端新发布。同步了 DLL、导入库、公共头和许可证。

默认保留 gamma 0.85 与原透明合成路径，没有启用对比界面的候选参数。Pulse、LUMENUI 原先未生效的 regular_optical_weight=0.06 改为 0，避免新版实际应用此字段后意外增粗。

| 项目 | 更新及验证 |
| --- | --- |
| pulse | 更新第三方 Release SDK，主程序及文字编辑测试构建通过，52 项检查通过；测试实际加载 build/lumatext.dll，哈希匹配。 |
| LumaShot | 更新 Release SDK 与自带源码快照，构建和安装包生成通过；定向测试 4/5 组通过。标注测试的 3 项失败使用原 DLL 对照仍相同，文字栅格化、混合 DPI、裁剪和内存检查通过。 |
| LUMENUI | 更新 Debug/Release SDK、Debug 导入配置和 SHA256SUMS；构建无 warning/error，visual、anim、api、perf 全部退出 0。性能场景平均 5.761 ms/帧，最差 50.108 ms，平均满足 8 ms 要求；不是 GPU 完成时间。打开 Gallery 检查中英文、字号层级和滚动显示，实际加载 build/lumatext.dll。 |
| SHOWBOX-LUMEN | 更新 Debug/Release SDK；AutoCAD 2027 Debug 编译通过；覆盖层 23 个用例、991 条断言通过，包含真实 LumaText 的多 DPI 中文换行。ARX 与测试输出 Debug DLL 哈希匹配。 |

## 产物

- Pulse：`C:/Users/SS/Desktop/pulse/build/pulse.exe`
- LUMEN 示例：`C:/Users/SS/Desktop/LUMENUI/build/lumen_gallery.exe`
- ShowBox：`C:/Users/SS/Desktop/SHOWBOX-LUMEN/cmake-build-debug/2027/ShowBox.arx`
- LumaShot 安装包：`C:/Users/SS/Desktop/LumaShot/dist/LumaShot-Setup.exe`
- 安装包 SHA-256：`FA80566E7FC979F55FA059EA91F9C6012D0E2280976F6DC6A1549DBC323493A8`

LumaText Release DLL SHA-256：`6FD80E3BD7BEFF1B2FC044DB0098F61598B0AA8750F6D8B6873566124F7F3D32`

LumaText Debug DLL SHA-256：`DE5F6697381F11A380B6D1162A0AEA2E8A9FE2AB26C57FD52C12B985EF47C829`

## 范围和限制

- 此次更新桌面项目及构建产物，没有安装新程序、覆盖 Program Files 中的已安装版或重启用户进程。已运行的 Pulse 安装版仍使用安装目录旧 DLL。
- Pulse 远端 CI 的 SDK pin 未修改：其 Windows 8.1 流程使用静态运行库并拒绝 MSVC 动态运行库导入，本次 SDK 使用 MD，需要另行制作和验证兼容包。
- ShowBox 未做 AutoCAD 内加载、交互或卸载验证，也未构建应用 Release 版本。
- LUMEN 的实机检查限于文字显示与滚动，未宣称完整 IME、鼠标手感及多显示器交互验收。
- LumaShot 原有失败为便签预设对比度、混合背景可读底色、标签文字颜色导出像素；旧 DLL 对照证据和安装包校验在该项目 `build/lumatext-sdk-update-verification.json`。
- 保留各项目已有无关修改，未提交或发布。
