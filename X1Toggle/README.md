# CLaunch 插件：XButton1 Toggle

用鼠标**侧键 X1（XButton1）关闭已显示的 CLaunch 窗口**。

配合 CLaunch 自身的鼠标钩子设置（X1 → 显示 CLaunch），实现：

- CLaunch **隐藏时**按 X1：事件原样放行，由 CLaunch 自己的钩子显示窗口（行为与现在完全一致）
- CLaunch **显示时**按 X1：插件隐藏窗口，并吞掉该次点击，避免 CLaunch 的"显示"动作再次触发

侧键 X2（XButton2）以及 CLaunch 隐藏时的 X1 功能（如浏览器的"后退"）不受影响。

## 工作原理

- 插件在 `PRQ_INIT` 时安装自己的低级鼠标钩子 `WH_MOUSE_LL`（运行在 CLaunch 主线程），只拦截 XButton1 的按下/抬起
- 通过 `PEV_SHOW` / `PEV_HIDE` 事件维护窗口可见状态，并在初始化时用 `IsWindowVisible()` 做一次同步
- 显示状态下拦截 X1 → 调用 `CpiHideWindow()` 关闭窗口，`return 1` 吞掉事件；隐藏状态下放行，由 ClHook.dll 完成显示
- **钩子链顺序**：低级钩子"后装者优先"，而 ClHook.dll 自己的钩子会把 XButton1 事件消费掉，插件若装得早就永远收不到事件。因此插件在每次 `PEV_SHOW` / `PEV_HIDE` / `PEV_HOOK` 时重装一次钩子，确保自己始终处于链首
- 钩子回调使用 `MSLLHOOKSTRUCT`（低级钩子的真实结构；`MOUSEHOOKSTRUCTEX` 是 WH_MOUSE 用的，字段偏移不同，混用会读错 `mouseData`）
- `PRQ_EXIT` 时卸载钩子

## 使用条件

- CLaunch 选项中已将鼠标侧键 X1 绑定为"显示 CLaunch"（插件只负责关闭方向的切换）
- CLaunch 核心版本 ≥ `SUPPORT_VERSION`（当前为 0x04020300，见 `X1Toggle.h`），版本过低时插件会初始化失败

## 编译

用 **Visual Studio 2026**（Community 免费版即可）打开 `X1Toggle.slnx`，选 **Release / x64**（或 Win32，须与 CLaunch.exe 位数一致）生成。输出位于：

```
x64\Release\X1Toggle.dll
```

命令行编译（任一版本的 MSVC 均可，代码为纯 Win32）：

```
msbuild X1Toggle.vcxproj /p:Configuration=Release /p:Platform=x64
```

## 安装

1. 将 `X1Toggle.dll` 复制到 CLaunch 的 **Plugins** 文件夹
2. 启动 CLaunch，在 **选项 → 插件** 中勾选启用 "XButton1 Toggle"
3. 现在窗口显示时按 X1 即可关闭

## 调试

1. 复制 `CLaunch.exe` 和 `ClHook.dll` 到插件的 Debug 输出目录
2. 在项目调试设置中让 VS 启动 `CLaunch.exe`，命令行参数 `/n /l`（非驻留模式，便于反复重启）

## 文件说明

| 文件 | 说明 |
| --- | --- |
| `X1Toggle.cpp` | 插件实现（低级鼠标钩子 + PEV 事件跟踪） |
| `X1Toggle.h` | CLaunch 插件 SDK 头文件（与官方模板一致） |
| `X1Toggle.def` | 导出 `ClPluginEventHandler` |
| `X1Toggle.rc` | 版本信息资源 |
| `X1Toggle.vcxproj` / `X1Toggle.slnx` | VS 工程文件 |

## 备注

- 插件列表中显示的名称可在 `X1Toggle.cpp` 的 `PLUGIN_TITLE` 处修改
- 想改用 X2 触发：把 `X1Toggle.cpp` 中的 `XBUTTON1` 改为 `XBUTTON2`（并把消息判断中的逻辑保持不变）重新编译即可
- 该插件不注册按钮项、无设置界面，`PRQ_SETTING` 等请求按不支持处理（返回 FALSE）
- **诊断日志**：默认关闭。排查问题时把 `X1Toggle.cpp` 开头的 `X1T_DEBUG_LOG` 改为 `1` 重新编译，会在 DLL 旁边生成 `X1Toggle.log`（记录初始化、钩子安装、事件与每次 X1 拦截）。日志为带 BOM 的 UTF-8 编码，任何文本编辑器可直接阅读
- `test\smoketest.c` 是 DLL 加载/导出冒烟测试，`test\x1probe.c` 可模拟 X1 按键并统计 CLaunch 可见窗口数，用于无人工干预的端到端验证
