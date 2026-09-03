# HackFramework

Windows 游戏注入框架，通过 DLL 注入挂钩目标进程的图形 API，在游戏画面上叠加 ImGui 菜单，驱动用户自定义功能模块。

## 特性

- 支持 DirectX 9/10/11/12、OpenGL、Vulkan
- ImGui 菜单叠加
- 每个功能独立后台线程，互不阻塞
- 无锁双缓冲（`LockFreeBuffer`）实现线程间零等待数据传递
- 开发者可控制是否启用后台线程及更新频率
- 禁用时线程通过 `condition_variable` 阻塞，零 CPU 开销
- 支持动态重挂钩（窗口重建时自动恢复）

## 项目结构

```
HackFramework/
├── dllmain.cpp                 DLL 入口
├── CMakeLists.txt              根构建配置
├── vcpkg.json                  依赖声明
│
├── backend/                    渲染后端
│   ├── include/backend/
│   │   ├── IRenderBackend.h    渲染后端接口
│   │   ├── BackendFactory.h    后端工厂
│   │   └── DX*_Backend.h       各图形 API 后端
│   └── src/                    后端实现
│
├── features/                   功能模块
│   ├── include/
│   │   ├── core/               框架层
│   │   │   ├── IFeature.h          功能接口
│   │   │   ├── BufferedFeature.h   无锁双缓冲基类
│   │   │   └── FeatureManager.h    功能调度器
│   │   └── impl/               业务层（你的代码）
│   │       └── Features.h          集中注册入口
│   └── src/
│       ├── core/               框架实现
│       └── impl/               业务实现
│
├── menu/                       ImGui 菜单
│   └── src/Menu.cpp
│
└── utils/                      工具库
    └── include/utils/
        ├── Logger.h            日志（spdlog）
        ├── InputHook.h         输入钩子
        ├── LockFreeBuffer.h    无锁双缓冲
        ├── ProcessWindow.h     窗口查找
        └── DllHelper.h         DLL 卸载
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

用 DLL 注入器将对应后端的 DLL 注入到目标进程。

| 按键 | 功能 |
|------|------|
| `INSERT` | 显示/隐藏菜单 |
| `HOME` | 重新挂钩 |
| `END` | 卸载 DLL |

## 添加功能

所有业务代码写在 `features/` 下的 `impl/` 目录中。

### 纯渲染功能（无后台线程）

```cpp
// features/include/impl/CrosshairFeature.h
#pragma once
#include "core/BufferedFeature.h"
#include <imgui.h>

struct CrosshairSnapshot { float size = 10.f; ImU32 color = IM_COL32(0,255,0,255); };

class CrosshairFeature : public Feature::BufferedFeature<CrosshairSnapshot>
{
public:
    const char* GetName() const override { return "Crosshair"; }
    bool& GetEnabled() override { return m_enabled; }

    bool Initialize() override { return true; }
    void Shutdown() override {}

    void OnUpdate() override
    {
        CrosshairSnapshot snap;
        snap.size = m_size;
        snap.color = IM_COL32(m_color[0]*255, m_color[1]*255, m_color[2]*255, 255);
        Publish(snap);
    }

    void OnDraw() override
    {
        auto s = Read();
        if (!s) return;
        auto* dl = ImGui::GetWindowDrawList();
        ImVec2 c(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
        dl->AddLine(ImVec2(c.x - s->size, c.y), ImVec2(c.x + s->size, c.y), s->color, 2.f);
        dl->AddLine(ImVec2(c.x, c.y - s->size), ImVec2(c.x, c.y + s->size), s->color, 2.f);
    }

    void DrawOptions() override
    {
        ImGui::SliderFloat("Size", &m_size, 2.f, 50.f);
        ImGui::ColorEdit3("Color", m_color);
    }

    bool HasUpdateThread() const override { return false; }  // 纯渲染，无后台线程

private:
    bool m_enabled = false;
    float m_size = 10.f;
    float m_color[3] = {0.f, 1.f, 0.f};
};
```

### 带后台线程的功能

```cpp
// features/include/impl/AimbotFeature.h
#pragma once
#include "core/BufferedFeature.h"
#include <imgui.h>

struct AimbotSnapshot { float fov = 10.f; float smooth = 5.f; };

class AimbotFeature : public Feature::BufferedFeature<AimbotSnapshot>
{
public:
    const char* GetName() const override { return "Aimbot"; }
    bool& GetEnabled() override { return m_enabled; }

    bool Initialize() override { return true; }
    void Shutdown() override {}

    void OnUpdate() override
    {
        AimbotSnapshot snap;
        snap.fov = m_fov;
        snap.smooth = m_smooth;
        Publish(snap);
    }

    void OnDraw() override
    {
        auto s = Read();
        if (!s) return;
        ImGui::Text("FOV: %.1f  Smooth: %.1f", s->fov, s->smooth);
    }

    void DrawOptions() override
    {
        ImGui::SliderFloat("FOV", &m_fov, 1.f, 30.f);
        ImGui::SliderFloat("Smooth", &m_smooth, 1.f, 20.f);
    }

    bool HasUpdateThread() const override { return true; }  // 启用后台线程

private:
    bool m_enabled = false;
    float m_fov = 10.f;
    float m_smooth = 5.f;
};
```

### 注册功能

```cpp
// features/src/impl/Features.cpp
#include "impl/Features.h"
#include "core/FeatureManager.h"
#include "impl/CrosshairFeature.h"
#include "impl/AimbotFeature.h"

namespace Feature
{
    void RegisterAll()
    {
        Manager::Get().Register(new CrosshairFeature());
        Manager::Get().Register(new AimbotFeature());
    }
}
```

### 自定义更新频率

```cpp
class SlowFeature : public Feature::BufferedFeature<SlowSnapshot>
{
    bool HasUpdateThread() const override { return true; }
    float GetUpdateIntervalMs() const override { return 1000.f / 10.f; }  // 10次/秒
};
```

## 架构设计

### 双线程模型

每个 feature 可选择拥有独立后台线程：

```
后台线程（feature 独享）       渲染线程（每帧一次）
    │                              │
    ├─ OnUpdate()                  ├─ OnDraw()
    │   ├─ 读游戏内存              │   ├─ Read() 获取快照
    │   ├─ 计算逻辑                │   └─ ImGui 绘制
    │   └─ Publish(快照)           │
    │        │                     │
    │        └─ LockFreeBuffer ────┘
    │           无锁双缓冲          │
    ├─ sleep(intervalMs)           │
    └─ 循环...                     │
```

### IFeature 接口

| 方法 | 线程 | 默认 | 说明 |
|------|------|------|------|
| `GetName()` | 任意 | — | 功能名称 |
| `GetEnabled()` | 任意 | — | 勾选状态（`atomic<bool>`） |
| `Initialize()` | 主线程 | — | 一次性初始化 |
| `Shutdown()` | 主线程 | — | 释放资源 |
| `OnUpdate()` | 后台线程 | — | 读数据 + 计算 + Publish |
| `OnDraw()` | 渲染线程 | — | Read 快照 + ImGui 绘制 |
| `DrawOptions()` | 渲染线程 | — | 配置面板 |
| `GetUpdateIntervalMs()` | — | `0` | sleep 毫秒数，0 = 框架默认（~6.67ms，150次/秒） |
| `HasUpdateThread()` | — | `false` | 是否启用后台线程 |

### 生命周期

```
DllMain(DLL_PROCESS_ATTACH)
  └─ OnProcessAttach()
       ├─ Logger::Init()
       ├─ MH_Initialize()
       └─ StartHooks()
            ├─ CreateBackend()
            ├─ FindProcessWindow()
            ├─ backend->Initialize()
            │    ├─ Menu::Initialize()       ImGui 上下文 + 注册 Feature + 启动线程
            │    └─ 挂钩 Present/ResizeBuffers
            └─ Input::Install()

WndProc 钩子：
  INSERT → 切换菜单
  HOME   → 重新挂钩
  END    → 卸载
```

## 依赖

- [imgui](https://github.com/ocornut/imgui) — GUI 渲染
- [minhook](https://github.com/TsudaKagework/minhook) — API 钩子
- [spdlog](https://github.com/gabime/spdlog) — 日志

通过 [vcpkg](https://github.com/microsoft/vcpkg) 管理，三元组 `x64-windows-static`。
