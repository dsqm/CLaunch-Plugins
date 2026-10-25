# CLaunch Plugins

CLaunch 插件集合仓库：每个插件一个独立子目录，互不依赖、可单独构建。

## 插件列表

| 插件 | 目录 | 说明 |
| --- | --- | --- |
| XButton1 Toggle | [X1Toggle/](X1Toggle/) | 鼠标侧键 X1 关闭已显示的 CLaunch 窗口，与 CLaunch 自身的"X1 显示"动作配合成完整的显示/隐藏切换 |
| Auto-close Unfocused Dialogs | [AutoCloseDialog/](AutoCloseDialog/) | 对话框（项目属性等）失去焦点时自动关闭，避免残留后台卡住 CLaunch 无法召唤 |
| App Hotkeys | [AppHotkeys/](AppHotkeys/) | CLaunch 窗口显示期间按自定义热键直接启动对应程序（最多 8 条映射，设置页配置），无需切换到程序所在分类 |

## 支持
赞助或支持该项目👇  
https://docs.qq.com/aio/DRWtMY3FQS0ZHRGRG


## 通用约定

- 插件基于官方插件开发包（CLaunch Plugin SDK：单一导出函数 `ClPluginEventHandler` + CPI 函数表）开发
- 各插件目录内自带 SDK 头文件（`X1Toggle.h` 即官方模板的 `CLaunchPlugin.h`），与其他插件互不影响
- 版本号自动生成：`build.bat` 取各插件最后一次提交的日期，生成 `<插件>/version.h`（版本号即该插件的最近更新日期），`.rc` 的 VERSIONINFO（DLL 右键属性）和插件列表「关于此插件」弹窗共用，无需手动维护
- 构建产物不入库：各插件目录下的 `x64/`、`Win32/`、根目录 `bin/`、测试编译产物均被 `.gitignore` 排除
- 安装方式通用：将构建出的 DLL 复制到 CLaunch 的 `Plugins` 目录，在 CLaunch 选项 → 插件 中启用
- 各插件的详细说明见其目录内的 README

## 构建

一键构建全部插件（Win32 + x64）：仓库根目录执行 `build.bat`。脚本自动定位 MSVC（vswhere，无需开发者命令行），输出到 `bin\Win32\` 与 `bin\x64\`（DLL 位数须与 CLaunch.exe 一致）。

也可用 Visual Studio 2022/2026 打开各插件的 `.slnx` 单独构建。源码含中文，三个 `.vcxproj` 已统一加 `/utf-8`。

## CI（GitHub Actions）

- `build`：每次推送编译全部插件，产物保留在 Actions artifact
- `nightly`：每天北京时间 0 点检查，有新提交就把最新构建发布到固定 tag `nightly` 的预发布上（无新提交则跳过）
- `auto-release`：15 天无推送自动发布正式 Release（tag = vYYYY.MM.DD）
- 附件：一个插件一个压缩包，以该插件版本号命名（如 `AppHotkeys-2026.10.25.zip`），内含 Win32 与 x64 两个 DLL，按 CLaunch 位数取用；nightly 附带全部插件，正式 Release 只附带本次有更新的插件（未更新的不提供附件，说明中明确标注）
- Release 说明由 `.github/scripts/plugin-notes.sh` 生成，逐插件列出：本次更新了哪些、哪些没更新（版本号 = 各插件最近更新日期，与 DLL 属性一致），时间均为北京时间

