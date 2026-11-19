# HackFramework

一个基于 Dear ImGui 的 Windows 游戏覆盖层（overlay）框架。编译产物是 DLL，注入目标进程后 hook 图形 API 的 Present/EndScene，在游戏画面上绘制菜单，并把各个功能（feature）解耦到独立后台线程中运行。

支持的后端：

| 后端 | 说明 | 产物（MinGW / MSVC） |
| --- | --- | --- |
| DX9 | Direct3D 9 `Present` / `PresentEx` / `SwapChain::Present` | `libHackFramework_DX9.dll` / `HackFramework_DX9.dll` |
| DX10 | DXGI `Present` + D3D10 探测 | `..._DX10.dll` |
| DX11 | DXGI `Present` / `Present1` + D3D11 探测 | `..._DX11.dll` |
| DX12 | DXGI `Present` + 命令队列 hook | `..._DX12.dll`（仅 x64） |
| OPENGL | `wglSwapBuffers` | `..._OPENGL.dll` |
| VULKAN | `vkQueuePresentKHR` | `..._VULKAN.dll` |

**每个 DLL 只包含一种后端**，在编译期决定，注入前需要确认游戏使用的图形 API。

## 功能概览

- 六大渲染后端，统一的 `IRenderBackend` 生命周期：启动时安装 hook，第一次 Present 时绑定游戏真实的 swap chain，设备替换/窗口重建后自动重绑。
- 无窗口（composition）swap chain 支持：`OutputWindow` 为空时按进程和分辨率查找窗口。
- ImGui 菜单：`INSERT` 显示/隐藏，`END` 安全卸载（停渲染、撤 hook、`FreeLibrary`）。
- Feature 框架：每个功能一个独立后台线程，勾选才唤醒，未勾选时条件变量挂起、零 CPU；渲染线程只负责画。
- 无锁双缓冲：后台线程 `Publish` 快照，渲染线程 `Read` 最新快照，互不阻塞。
- MinHook 封装：只管理自己安装的 hook，可与进程里其他 MinHook 使用者共存。
- 异常保护：渲染回调里的异常不会把游戏带崩，覆盖层会自动降级/停止。
- 静态链接（MinGW 下 `libstdc++`/`libgcc` 静态），单个 DLL 即可注入。

## 目录结构

```
HackFramework/
├── CMakeLists.txt              # 顶层构建配置（后端开关、编译/链接选项）
├── vcpkg.json                  # 依赖清单
├── backend/                    # DLL 入口、生命周期、各图形后端 hook
│   ├── include/backend/
│   └── src/
│       ├── dllmain.cpp         # DllMain，起工作线程
│       ├── Lifecycle.cpp       # 日志、MinHook、后端初始化/卸载
│       ├── BackendFactory.cpp  # 按编译宏选择唯一后端
│       └── *_Backend.cpp       # DX9/10/11/12、OpenGL、Vulkan
├── menu/                       # ImGui 上下文、菜单窗口、热键
├── feature/                    # IFeature 接口、调度器、注册入口
│   ├── include/feature/core/   # IFeature / FeatureManager / BufferedFeature
│   ├── include/feature/impl/   # Features.h（注册入口声明）
│   └── src/
├── core/                       # 故障保护、延迟 teardown、Win32 工具
└── utils/                      # Logger、HookHelper、InputHook、DllHelper 等
```

## 构建

### 依赖

- CMake ≥ 4.1
- C++20 编译器：MSVC，或 MinGW-w64（`x86_64-w64-mingw32-g++`）
- [vcpkg](https://github.com/microsoft/vcpkg)：`spdlog`、`imgui`（含全部 binding）、`minhook`

依赖已在 `vcpkg.json` 中声明，开启 manifest 模式时由 vcpkg 自动安装。

### MinGW（macOS / Linux 交叉编译）

```bash
cmake -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake" \
  -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
  -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
  -DVCPKG_TARGET_TRIPLET=x64-mingw-static \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build -j
```

MinGW 分支默认开启：

- `-ffunction-sections -fdata-sections` + `-Wl,--gc-sections`：死代码消除；
- `-Wl,--exclude-all-symbols`：关闭 DLL 的自动导出（否则 ld 会导出所有全局符号，`--gc-sections` 失效且 `.edata` 多出上百 KB）；
- `-static -static-libgcc -static-libstdc++`：静态运行时，单文件注入；
- Release 下 `-s`：剥离符号。

### MSVC（Windows）

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static

cmake --build build --config Release
```

MSVC 分支使用 `x64-windows-static` 三元组和静态 CRT（`MultiThreaded`）。

### 32 位目标

顶层 `CMakeLists.txt` 里修改：

```cmake
set(BUILD_ARCH "x86")
```

同时把 vcpkg 三元组换成 `x86-mingw-static` / `x86-windows-static`。x86 下不构建 DX12 后端。

### 产物位置

所有后端 DLL 输出到**构建根目录**（`backend/` 的上一级）：

```
build/libHackFramework_DX11.dll
build/libHackFramework_DX11_debug.dll   # Debug 配置带 _debug 后缀
```

MinGW 会加 `lib` 前缀；MSVC 不加。

## 使用

1. 确认游戏使用的图形 API，选择对应 DLL。
2. 用 64 位 DLL 注入器把 DLL 注入游戏进程（DLL 与游戏位数必须一致）。
3. 注入后等待游戏第一次 Present，覆盖层自动绑定窗口。
4. 热键：
   - `INSERT`：显示 / 隐藏菜单；
   - `END`：安全卸载（停止渲染、卸载 hook、释放 DLL）。

注意事项：

- 日志：Debug 构建会弹出 "Debug Logger Console" 控制台；Release 构建关闭日志。
- 带反作弊的联机游戏会检测注入，仅建议在自建测试程序或单机环境使用。
- 菜单隐藏时 `Menu::Render()` 会提前返回，`OnDraw` 不会执行。如果功能需要在菜单关闭时继续绘制（如 ESP），需要调整这段逻辑。
- Vulkan 后端目前把菜单命令提交到 Present 队列。若游戏的 Present 队列不支持 `VK_QUEUE_GRAPHICS_BIT`（少数引擎），菜单可能出现异常；参考项目 UniversalHookX 对此有 compute 队列回退，本框架尚未移植。

## 开发自己的功能

### 1. 实现 feature

继承 `Feature::IFeature`，或直接继承 `Feature::BufferedFeature<Snapshot>` 使用内置无锁双缓冲：

```cpp
#include "feature/core/BufferedFeature.h"

#include <imgui.h>

struct MySnapshot
{
    int value = 0;
};

class MyFeature : public Feature::BufferedFeature<MySnapshot>
{
public:
    const char* GetName() const override { return "My Feature"; }

    bool Initialize() override
    {
        // 安装需要的 hook，失败返回 false
        return true;
    }

    void Shutdown() override
    {
        // 卸载 hook、释放资源
    }

    bool HasUpdateThread() const override { return true; }              // 开启独立后台线程
    float GetUpdateIntervalMs() const override { return 1000.f / 60.f; } // 默认 150Hz

    void OnUpdate() override
    {
        // 后台线程：读游戏内存、计算，然后发布快照
        MySnapshot snapshot{};
        snapshot.value = 42;
        Publish(snapshot);
    }

    void OnDraw() override
    {
        // 渲染线程：只读取快照并绘制，不做重活
        if (auto snapshot = Read())
            ImGui::Text("value: %d", snapshot->value);
    }

    bool& GetEnabled() override { return m_enabled; }

    void DrawOptions() override
    {
        ImGui::SliderInt("Option", &m_option, 0, 100);
    }

private:
    bool m_enabled = false;
    int m_option = 0;
};
```

### 2. 注册

在 `feature/src/impl/Features.cpp` 的 `RegisterAll()` 中登记，必须在 `Manager::Start()` 之前：

```cpp
void Feature::RegisterAll()
{
    auto& manager = Feature::Manager::Get();
    manager.Register(std::make_unique<MyFeature>());
}
```

注册后，功能会自动出现在菜单里（checkbox + `DrawOptions` 面板），无需自己画菜单。

### 3. 安装 hook

使用 `utils/HookHelper.h`（MinHook 封装）：

```cpp
#include "utils/HookHelper.h"

bool Initialize() override
{
    return HookHelper::InstallHook("Game::Update", target, detour, &original);
}

void Shutdown() override
{
    HookHelper::UninstallHook("Game::Update", target);
}
```

## 运行时模型

| 线程 | 职责 |
| --- | --- |
| 游戏渲染线程（Present） | `Menu::Render()` → 绘制菜单 → `TickDraw()` → 已启用 feature 的 `OnDraw()` |
| feature 后台线程（每个 feature 一个） | 已启用时循环执行 `OnUpdate()`，每次结束按 `GetUpdateIntervalMs()` 休眠；未启用时条件变量挂起 |
| DLL 工作线程 | 初始化日志、MinHook、安装后端 hook；卸载时负责清理 |

- `Manager::Start()`：调用每个 feature 的 `Initialize()`，并为 `HasUpdateThread()` 为 true 的 feature 建线程；
- `Manager::Stop()`：停止并 join 所有线程，调用 `Shutdown()`，释放 feature；
- `BufferedFeature::Publish/Read`：写端（后台线程）与读端（渲染线程）通过 `Utils::LockFreeBuffer` 交换不可变快照。

## 参考项目

本框架在发展过程中参考了以下两个 MIT 项目：

- [UniversalHookX](https://github.com/bruhmoment21/UniversalHookX)（MIT）：早期版本的骨架，包括每种后端独立目录、dummy device 取 vtable 的 hook 方式、`GetProcessWindow` 窗口枚举、WndProc 子类化与 `INSERT` / `END` 热键等。
- [Universal-Dear-ImGui-Hook](https://github.com/Sh0ckFR/Universal-Dear-ImGui-Hook)（MIT）：当前后端的生命周期设计、swap chain 绑定与重绑、composition（无窗口）swap chain、MinHook 共享等实现参考。

## 致谢

- [Dear ImGui](https://github.com/ocornut/imgui)：界面渲染。
- [MinHook](https://github.com/TsudaKageyu/MinHook)：函数 hook。
- [spdlog](https://github.com/gabime/spdlog)：日志。
