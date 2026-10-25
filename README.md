# CLaunch Plugins

CLaunch 插件集合仓库：每个插件一个独立子目录，互不依赖、可单独构建。

## 插件列表

| 插件 | 目录 | 说明 |
| --- | --- | --- |
| XButton1 Toggle | [X1Toggle/](X1Toggle/) | 鼠标侧键 X1 关闭已显示的 CLaunch 窗口，与 CLaunch 自身的"X1 显示"动作配合成完整的显示/隐藏切换 |
| Auto-close Unfocused Dialogs | [AutoCloseDialog/](AutoCloseDialog/) | 对话框（项目属性等）失去焦点时自动关闭，避免残留后台卡住 CLaunch 无法召唤 |

## 通用约定

- 插件基于官方插件开发包（CLaunch Plugin SDK：单一导出函数 `ClPluginEventHandler` + CPI 函数表）开发
- 各插件目录内自带 SDK 头文件（`X1Toggle.h` 即官方模板的 `CLaunchPlugin.h`），与其他插件互不影响
- 构建产物不入库：各插件目录下的 `x64/`、`Win32/`、测试编译产物均被 `.gitignore` 排除
- 安装方式通用：将构建出的 DLL 复制到 CLaunch 的 `Plugins` 目录，在 CLaunch 选项 → 插件 中启用
- 各插件的详细说明见其目录内的 README
