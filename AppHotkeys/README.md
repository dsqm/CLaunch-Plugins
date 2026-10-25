# CLaunch 插件：App Hotkeys

**CLaunch 窗口显示期间，按自定义热键直接启动对应程序**，无需切换到程序所在的分类页面——弥补 CLaunch 自带热键仅在当前分类生效的局限。

- 最多 **8 条**"热键 → 程序"映射，在设置页（选项 → 插件 → App Hotkeys → 设置）中逐行配置：热键框内直接按键录入，路径支持环境变量与相对路径
- 仅在 **CLaunch 窗口显示时**拦截按键；窗口隐藏时所有按键原样放行，不影响其他软件
- 启动目标程序后自动关闭 CLaunch 窗口（与正常执行按钮项的行为一致）
- 热键或路径留空的行不生效；路径无法解析时按键放行

## 工作原理

- `PRQ_INIT` 时安装 `WH_KEYBOARD_LL` 低级键盘钩子（运行在 CLaunch 主线程），并在 `PEV_SHOW` / `PEV_HIDE` / `PEV_HOOK` 事件时重装钩子，保持位于钩子链最前（抢先于 ClHook.dll）
- 通过 `PEV_SHOW` / `PEV_HIDE` 事件跟踪窗口可见性，仅在窗口显示时匹配热键
- 按键被吞掉前会启动对应程序；按住不放的重复按键只触发一次；设置通过 `CpiSetStrData` / `CpiSetIntData` 存入 CLaunch 配置，`PRQ_EXIT` 时卸载钩子

## 使用说明

- 热键在设置页的热键框内**直接按键**录入（如按 `Ctrl+Alt+E` 即录入该组合）
- 相对路径基准：CLaunch.exe 所在目录（找不到时尝试数据目录）；路径含空格无需加引号（路径与参数分栏填写）
- 全部行清空保存即可让插件完全不干预

## 编译

用 Visual Studio 2022/2026 打开 `AppHotkeys.slnx`，选 Release / x64（或 Win32，须与 CLaunch.exe 位数一致）生成；或命令行：

```
msbuild AppHotkeys.vcxproj /p:Configuration=Release /p:Platform=x64
```

注意 `AppHotkeys.rc` 为 UTF-16LE 编码（含中文界面文本），编辑后请保持编码。`test\rescheck.c` 可核对编译产物中的对话框/版本资源内容。

## 安装

1. 将 `AppHotkeys.dll` 复制到 CLaunch 的 **Plugins** 文件夹
2. 启动 CLaunch，在 选项 → 插件 中勾选启用 "App Hotkeys"
3. 打开其 设置 页逐行配置热键与程序，确定保存

## 诊断日志

默认关闭。把 `AppHotkeys.cpp` 开头的 `AHD_DEBUG_LOG` 改为 `1` 重新编译，会在 DLL 旁边生成 `AppHotkeys.log`（记录初始化、设置保存、按键捕获与启动决策）。日志为带 BOM 的 UTF-8 编码。
