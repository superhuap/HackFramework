//
// Created by superhuap on 2026/11/18.
// Ported from the previous HackFramework iteration.
//

#include "feature/core/FeatureManager.h"

#include <imgui.h>

#include "utils/Logger.h"

namespace Feature
{

    Manager& Manager::Get()
    {
        static Manager instance;
        return instance;
    }

    Manager::~Manager()
    {
        Stop();
    }

    void Manager::Register(std::unique_ptr<IFeature> feature)
    {
        if (!feature)
            return;

        std::lock_guard<std::mutex> lock(m_mutex);
        m_features.push_back(std::move(feature));
    }

    void Manager::Start()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Only the first Start() initializes: a second call would otherwise run
        // Initialize() again on every feature and then return early.
        if (m_running.exchange(true))
            return;

        for (const auto& owned : m_features)
        {
            IFeature* feature = owned.get();
            if (feature && !feature->Initialize())
                Logger::Error("[Feature] Initialize: '{}' failed", feature->GetName());
        }

        for (const auto& owned : m_features)
        {
            IFeature* feature = owned.get();
            if (!feature || !feature->HasUpdateThread())
                continue;

            // Insert first, then hand the stable node to the thread: the map is
            // never touched from the feature thread.
            auto [it, inserted] = m_threads.try_emplace(feature);
            if (!inserted && it->second.thread.joinable())
                continue;
            it->second.thread = std::thread(&Manager::TickUpdateLoop, this, feature, &it->second);
        }

        Logger::Debug("[Feature] Start: {} feature(s)", m_features.size());
    }

    void Manager::Stop()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_running.exchange(false))
                return;
        }

        for (auto& [feature, ft] : m_threads)
            NotifyFeature(feature);

        for (auto& [feature, ft] : m_threads)
        {
            if (ft.thread.joinable())
                ft.thread.join();
        }
        m_threads.clear();

        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& owned : m_features)
        {
            if (owned)
                owned->Shutdown();
        }
        m_features.clear();
        Logger::Debug("[Feature] Stop: done");
    }

    void Manager::TickUpdateLoop(IFeature* feature, FeatureThread* ft)
    {
        while (is_running())
        {
            {
                std::unique_lock<std::mutex> lock(ft->mtx);
                ft->cv.wait(lock, [&] {
                    return !is_running() || feature->IsEnabled();
                });
            }

            if (!is_running())
                break;

            feature->OnUpdate();

            float intervalMs = feature->GetUpdateIntervalMs();
            if (intervalMs <= 0.f)
                intervalMs = kDefaultUpdateIntervalMs;

            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(intervalMs)));
        }
    }

    void Manager::TickDraw()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& owned : m_features)
        {
            if (owned && owned->IsEnabled())
                owned->OnDraw();
        }
    }

    void Manager::DrawMenu()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_features.empty())
        {
            ImGui::Text("No features registered.");
            return;
        }

        for (const auto& owned : m_features)
        {
            IFeature* feature = owned.get();
            if (!feature)
                continue;

            bool enabled = feature->IsEnabled();
            const bool was_enabled = enabled;
            if (ImGui::Checkbox(feature->GetName(), &enabled))
                feature->SetEnabled(enabled);

            if (!was_enabled && enabled)
                NotifyFeature(feature);

            if (enabled)
            {
                ImGui::Indent();
                feature->DrawOptions();
                ImGui::Unindent();
            }
        }
    }

    bool Manager::is_running() const
    {
        return m_running.load();
    }

    void Manager::NotifyFeature(IFeature* feature)
    {
        auto it = m_threads.find(feature);
        if (it != m_threads.end())
        {
            std::lock_guard<std::mutex> lock(it->second.mtx);
            it->second.cv.notify_one();
        }
    }

} // namespace Feature
