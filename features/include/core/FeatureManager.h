//
// Created by superhuap on 2026/8/25.
//

#ifndef HACKFRAMEWORK_FEATUREMANAGER_H
#define HACKFRAMEWORK_FEATUREMANAGER_H

#include "core/IFeature.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Feature
{

    // 功能调度器。
    //
    // 生命周期挂在 Menu 上：
    //   - Start() 在 Menu::Initialize 时调用：对所有已注册 feature 调 Initialize，并为每个 feature 启动独立后台线程。
    //   - Stop()  在 Menu::Shutdown 时调用：停止所有后台线程(join)，对全部 feature 调 Shutdown 并释放。
    //
    // 每帧：
    //   - TickDraw() 在 Menu::Render 时调用：仅对 GetEnabled() 为 true 的 feature 调 OnDraw。
    //   - DrawMenu() 在菜单显示时调用：为每个 feature 生成 checkbox + 配置面板。
    //
    // 后台线程：
    //   - 每个 feature 拥有独立后台线程，互不阻塞。
    //   - 禁用时线程通过 condition_variable 阻塞挂起，零 CPU 开销。
    //   - 启用时由 cv.notify_one() 唤醒，每次 OnUpdate 后 sleep GetUpdateIntervalMs() 毫秒。
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

        bool is_running() const;

        ~Manager();

    private:
        Manager() = default;

        void TickUpdateLoop(IFeature* feature);
        void NotifyFeature(IFeature* feature);

        struct FeatureThread
        {
            std::thread thread;
            std::mutex mtx;
            std::condition_variable cv;
        };

        static constexpr float kDefaultUpdateIntervalMs = 1000.f / 150.f;  // ~6.67ms, 150次/秒

        std::mutex m_mutex;
        std::vector<IFeature*> m_features;
        std::unordered_map<IFeature*, FeatureThread> m_threads;
        std::atomic<bool> m_running{false};
    };

} // namespace Feature

#endif // HACKFRAMEWORK_FEATUREMANAGER_H