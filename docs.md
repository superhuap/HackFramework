# HackFramework 开发文档

## 项目概览

HackFramework 是一个 Windows 游戏注入框架，通过 DLL 注入挂钩目标进程的图形 API（DirectX 9/10/11/12、OpenGL、Vulkan），在游戏画面上叠加 ImGui 菜单，驱动用户自定义功能模块。

## 目录结构

```
HackFramework/
├── CMakeLists.txt              根构建配置
├── dllmain.cpp                 DLL 入口，生命周期管理
├── vcpkg.json                  依赖声明
├── config.h.in                 版本/日志配置模板
│
├── backend/                    渲染后端（框架）
│   ├── include/backend/
│   │   ├── IRenderBackend.h    渲染后端抽象接口
│   │   ├── BackendFactory.h    后端工厂
│   │   ├── MinHookHelper.h     MinHook 封装
│   │   ├── DX9_Backend.h
│   │   ├── DX10_Backend.h
│   │   ├── DX11_Backend.h
│   │   ├── DX12_Backend.h
│   │   ├── OPENGL_Backend.h
│   │   └── VULKAN_Backend.h
│   └── src/                    各后端实现 + 工厂
│
├── features/                   功能模块
│   ├── include/
│   │   ├── core/               框架层
│   │   │   ├── IFeature.h          功能抽象接口
│   │   │   ├── BufferedFeature.h   无锁双缓冲基类模板
│   │   │   └── FeatureManager.h    功能调度器
│   │   └── impl/               业务层（你的代码放这里）
│   │       └── Features.h          集中注册入口
│   └── src/
│       ├── core/               框架实现
│       │   └── FeatureManager.cpp
│       └── impl/               业务实现
│           └── Features.cpp        注册入口实现
│
├── menu/                       ImGui 菜单（框架）
│   ├── include/menu/Menu.h
│   └── src/Menu.cpp
│
└── utils/                      工具库（框架）
    ├── include/utils/
    │   ├── Logger.h            spdlog 日志封装
    │   ├── InputHook.h         WndProc 钩子（Insert/Home/End 按键）
    │   ├── ProcessWindow.h     窗口查找
    │   ├── DllHelper.h         DLL 卸载
    │   └── LockFreeBuffer.h    无锁双缓冲
    └── src/
```

## 构建

### 前置条件

- Windows，C++20
- [vcpkg](https://github.com/microsoft/vcpkg)
- CMake 3.20+
- Visual Studio 2022+ 或 CLion 2023+

### 产物

默认启用所有后端，每个后端生成一个独立 DLL，输出在 `backend/` 目录下：

```
backend/HackFramework_DX9.dll
backend/HackFramework_DX10.dll
backend/HackFramework_DX11.dll
backend/HackFramework_DX12.dll
backend/HackFramework_OPENGL.dll
backend/HackFramework_VULKAN.dll
```

## 使用

用 DLL 注入器（如 Process Hacker、Xenos 等）将对应后端的 DLL 注入到目标进程。

### 按键操作

| 按键 | 功能 |
|------|------|
| `INSERT` | 显示 / 隐藏 ImGui 菜单 |
| `HOME` | 重新挂钩（窗口重建时使用） |
| `END` | 卸载 DLL |

## 开发自己的功能

所有业务代码写在 `features/` 下的 `impl/` 目录中。

### 第一步：定义快照结构

在 `features/include/impl/` 下创建头文件，定义后台线程和渲染线程之间传递的数据：

```cpp
// features/include/impl/AimbotFeature.h
#pragma once

struct AimbotSnapshot
{
    bool active = false;
    float fov = 10.f;
    float smoothness = 5.f;
};
```

### 第二步：继承 BufferedFeature

使用框架提供的 `BufferedFeature<Snapshot>` 基类，它内置无锁双缓冲，自动处理后台线程到渲染线程的数据传递：

```cpp
// features/include/impl/AimbotFeature.h
#pragma once

#include "core/BufferedFeature.h"
#include "impl/AimbotSnapshot.h"

class AimbotFeature : public Feature::BufferedFeature<AimbotSnapshot>
{
public:
    const char* GetName() const override { return "Aimbot"; }
    bool& GetEnabled() override { return m_enabled; }

    bool Initialize() override;   // 资源分配
    void Shutdown() override;     // 资源释放

    void OnUpdate() override;     // 后台线程：读游戏内存 + 计算
    void OnDraw() override;       // 渲染线程：读快照 + ImGui 绘制
    void DrawOptions() override;  // 菜单配置面板

private:
    bool m_enabled = false;
    float m_fov = 10.f;
    float m_smoothness = 5.f;
};
```

### 第三步：实现

```cpp
// features/src/impl/AimbotFeature.cpp
#include "impl/AimbotFeature.h"

bool AimbotFeature::Initialize()
{
    // 初始化资源（读取基地址、创建线程等）
    return true;
}

void AimbotFeature::Shutdown()
{
    // 释放资源
}

void AimbotFeature::OnUpdate()
{
    // ===== 后台线程 =====
    // 读取游戏内存、计算瞄准目标等

    AimbotSnapshot snap;
    snap.active = m_enabled;
    snap.fov = m_fov;
    snap.smoothness = m_smoothness;

    // 发布快照，渲染线程会自动读取最新的一份
    Publish(snap);
}

void AimbotFeature::OnDraw()
{
    // ===== 渲染线程 =====
    // 读取快照，如果还没 Publish 过则为 nullptr
    auto snap = Read();
    if (!snap) return;

    ImGui::Text("FOV: %.1f", snap->fov);
    ImGui::Text("Smooth: %.1f", snap->smoothness);
}

void AimbotFeature::DrawOptions()
{
    // 菜单里的配置面板（仅菜单显示且功能启用时调用）
    ImGui::SliderFloat("FOV", &m_fov, 1.f, 30.f);
    ImGui::SliderFloat("Smoothness", &m_smoothness, 1.f, 20.f);
}
```

### 第四步：注册

在 `features/src/impl/Features.cpp` 中注册你的功能：

```cpp
#include "impl/Features.h"
#include "core/FeatureManager.h"
#include "impl/AimbotFeature.h"

namespace Feature
{
    void RegisterAll()
    {
        Manager::Get().Register(new AimbotFeature());
        // Manager::Get().Register(new EspFeature());
        // Manager::Get().Register(new AnotherFeature());
    }
}
```

注册完成后，重新编译即可在 ImGui 菜单中看到你的功能。

## 架构设计

### 双线程模型

```
后台线程（满速循环）          渲染线程（每帧一次）
    │                              │
    ├─ OnUpdate()                  ├─ OnDraw()
    │   ├─ 读游戏内存              │   ├─ Read() 获取最新快照
    │   ├─ 计算逻辑                │   └─ ImGui 绘制
    │   └─ Publish(快照)           │
    │        │                     │
    │        └─ LockFreeBuffer ────┘
    │           无锁双缓冲          │
```

- `OnUpdate()` 和 `OnDraw()` 通过 `LockFreeBuffer` 解耦
- 读端零等待、零拷贝，适合"后台读取、渲染绘制"的分离场景

### 生命周期

```
DllMain(DLL_PROCESS_ATTACH)
  └─ OnProcessAttach()
       ├─ Logger::Init()           分配控制台，初始化 spdlog
       ├─ MH_Initialize()          初始化 MinHook
       └─ StartHooks()
            ├─ CreateBackend()     根据编译宏创建 DX11/Vulkan 等后端
            ├─ FindProcessWindow() 查找目标窗口
            ├─ backend->Initialize()
            │    ├─ Menu::Initialize()       创建 ImGui 上下文 + 注册所有 Feature + 启动后台线程
            │    └─ 挂钩 Present/ResizeBuffers 等图形 API
            └─ Input::Install()    安装 WndProc 钩子

WndProc 钩子：
  INSERT → 切换菜单显示
  HOME   → 重新挂钩（窗口重建）
  END    → 卸载 DLL

DllMain(DLL_PROCESS_DETACH)
  └─ PrepareUnload()
       ├─ StopHooks()              停用所有 hook
       ├─ MH_Uninitialize()        释放 MinHook
       └─ Logger::Shutdown()       关闭日志
```

### Feature 接口

实现 `Feature::IFeature` 接口或继承 `Feature::BufferedFeature<T>` 模板：

| 方法 | 调用线程 | 调用时机 | 说明 |
|------|----------|----------|------|
| `GetName()` | 任意 | 菜单显示时 | 返回功能名称（显示在 checkbox 旁） |
| `GetEnabled()` | 任意 | 勾选状态判断 | 返回对 `m_enabled` 的引用 |
| `Initialize()` | 主线程 | Manager::Start | 一次性初始化 |
| `Shutdown()` | 主线程 | Manager::Stop | 释放资源 |
| `OnUpdate()` | 后台线程 | 每 tick（启用时） | 读数据 + 计算 + Publish 快照 |
| `OnDraw()` | 渲染线程 | 每帧（启用时） | Read 快照 + ImGui 绘制 |
| `DrawOptions()` | 渲染线程 | 菜单显示且启用 | 配置面板（颜色、大小等） |

### 键盘快捷键

| 按键 | 行为 | 定义位置 |
|------|------|----------|
| `INSERT` | 切换菜单可见性 | `utils/src/InputHook.cpp` |
| `HOME` | 触发重新挂钩 | `utils/src/InputHook.cpp` |
| `END` | 卸载 DLL | `utils/src/InputHook.cpp` |

## 日志

基于 spdlog 的异步日志，DLL 注入后自动分配控制台窗口。

```cpp
#include "utils/Logger.h"

LOG_INFO("初始化完成");
LOG_ERROR("读取内存失败: 0x{:X}", address);
LOG_WARN("窗口未找到，等待中...");
```

- `ENABLE_LOGGING` 开启时显示控制台，关闭时隐藏
- `ENABLE_LOGGING_COLOR` 开启时日志带颜色

## 注意事项

1. **每个后端一个 DLL**：DX11 游戏用 `HackFramework_DX11.dll`，Vulkan 游戏用 `HackFramework_VULKAN.dll`，不能混用。
2. **静态 CRT**：三元组为 `x64-windows-static`，所有依赖和 CRT 静态链接，无运行时依赖。
3. **只改 `impl/` 目录**：添加新功能时只在 `features/` 的 `impl/` 下操作，不要修改 `core/` 下的框架代码。
4. **OnUpdate 不要阻塞**：后台线程满速循环，`OnUpdate()` 中避免长时间阻塞操作，否则会拖慢所有功能的更新。
5. **OnDraw 不要做重活**：渲染线程里只做 ImGui 绘制，不要读游戏内存或做计算。
