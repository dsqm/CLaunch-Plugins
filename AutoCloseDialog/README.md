# CLaunch 插件：Auto-close Unfocused Dialogs

**CLaunch 的对话框（项目属性等）失去焦点时自动关闭**，解决对话框藏在后台导致 CLaunch 无法被召唤的问题。

## 解决的问题

打开按钮的"项目属性"等对话框后，如果焦点转到其他程序，对话框会残留在后台：主窗口被模态对话框卡住，再按侧键 X1 / 热键也召唤不出来，必须自己找到藏在后面的对话框关掉才行。

本插件在焦点离开 CLaunch 进程的瞬间，自动关闭 CLaunch 残留的对话框（等效于按了取消），保证 CLaunch 随时可以被正常召唤。

## 工作原理

- `PRQ_INIT` 时安装 `SetWinEventHook(EVENT_SYSTEM_FOREGROUND, ..., WINEVENT_OUTOFCONTEXT)`，在 CLaunch 主线程接收系统前台切换事件
- 前台窗口不属于 CLaunch 进程时，`EnumWindows` 遍历该进程的顶层窗口，把满足以下条件的窗口 `PostMessage(WM_CLOSE)`：
  - 可见
  - 不是主窗口（`CpiGetHandle()`）
  - 窗口类为标准对话框类 `#32770`
- `PRQ_EXIT` 时 `UnhookWinEvent` 释放钩子

## 不关闭的窗口

标题包含以下关键词的窗口一律不关，并且**这些窗口拥有的对话框也一并保留**（判定沿 owner 链上溯，直到主窗口为止）——例如从"图标设置"窗口派生的文件选择框：

| 关键词 | 覆盖的窗口 |
| --- | --- |
| `选项` / `選項` | CLaunch 选项（简中/繁中） |
| `オプション` / `設定` | CLaunch 选项及其子窗口（日语） |
| `Options` / `Opzioni` / `Opciones` / `옵션` | CLaunch 选项（英/意/西/韩） |
| `图标设置` / `圖示` / `Icon` | 图标设置窗口（简中/繁中/英语），需与资源管理器来回切换挑选图标 |

其余对话框（项目属性、消息框、关于窗口等）失焦即关闭，等效于按取消——未确认的编辑会丢失，这是预期行为。关键词列表在 `AutoCloseDialog.cpp` 的 `s_pszKeepTitles` 中，可自行增删。

## 使用条件与说明

- CLaunch 核心版本 ≥ 0x04020300
- CLaunch 未运行时插件不产生任何效果；卸载插件后行为恢复原样
- 若 CLaunch 选项也想要"失焦自动关闭"，把上面关键词列表清空即可（有丢失未保存设置的风险）

## 编译

用 **Visual Studio 2022/2026** 打开 `AutoCloseDialog.slnx`，选 **Release / x64**（或 Win32，须与 CLaunch.exe 位数一致）生成；或命令行：

```
msbuild AutoCloseDialog.vcxproj /p:Configuration=Release /p:Platform=x64
```

输出位于 `x64\Release\AutoCloseDialog.dll`。

## 安装

1. 将 DLL 复制到 CLaunch 的 **Plugins** 文件夹
2. 启动 CLaunch，在 选项 → 插件 中勾选启用 "Auto-close Unfocused Dialogs"

## 诊断日志

默认关闭。把 `AutoCloseDialog.cpp` 开头的 `ACD_DEBUG_LOG` 改为 `1` 重新编译，会在 DLL 旁边生成 `AutoCloseDialog.log`（记录初始化与每次关闭的窗口标题/类名）。日志为带 BOM 的 UTF-8 编码，任何文本编辑器可直接阅读。
