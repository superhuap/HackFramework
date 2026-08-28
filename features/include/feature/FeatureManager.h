//
// Created by superhuap on 2026/8/25.
//

#ifndef HACKFRAMEWORK_FEATUREMANAGER_H
#define HACKFRAMEWORK_FEATUREMANAGER_H

#include "feature/IFeature.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace Feature
{

    // 功能调度器。
    //
    // 生命周期挂在 Menu 上：
    //   - Start() 在 Menu::Initialize 时调用：对所有已注册 feature 调 Initialize，并启动后台线程。
    //   - Stop()  在 Menu::Shutdown 时调用：停止后台线程(join)，对全部 feature 调 Shutdown 并释放。
    //
    // 每帧：
    //   - TickDraw() 在 Menu::Render 时调用：仅对 GetEnabled() 为 true 的 feature 调 OnDraw。
    //   - DrawMenu() 在菜单显示时调用：为每个 feature 生成 checkbox + 配置面板。
    //
    // 后台线程：
    //   - TickUpdateLoop() 满速循环，仅对 GetEnabled() 为 true 的 feature 调 OnUpdate。
    class Manager
    {
    public:
        static Manager& Get();

        Manager(const Manager&) = delete;
        Manager& operator=(const Manager&) = delete;

        // 注册一个功能实例（拥有权转移给 Manager）。需在 Start() 之前调用。
        void Register(IFeature* feature);

        void Start();
        void Stop();

        void TickDraw();
        void DrawMenu();

        ~Manager();

    private:
        Manager() = default;

        void TickUpdateLoop();

        std::mutex m_mutex;
        std::vector<IFeature*> m_features;
        std::thread m_updateThread;
        std::atomic<bool> m_running{false};
    };

} // namespace Feature

#endif // HACKFRAMEWORK_FEATUREMANAGER_H