//
// Created by superhuap on 2026/8/25.
//

#include "features/core/FeatureManager.h"

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

    void Manager::Register(IFeature* feature)
    {
        if (!feature)
            return;

        std::lock_guard<std::mutex> lock(m_mutex);
        m_features.push_back(feature);
    }

    void Manager::Start()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        for (IFeature* feature : m_features)
        {
            if (feature && !feature->Initialize())
                LOG_ERROR("Feature '{}' failed to initialize", feature->GetName());
        }

        if (m_running.exchange(true))
            return;

        for (IFeature* feature : m_features)
        {
            if (feature && feature->HasUpdateThread())
                m_threads[feature].thread = std::thread(&Manager::TickUpdateLoop, this, feature);
        }

        LOG_INFO("Feature manager started ({} feature(s))", m_features.size());
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
        for (IFeature* feature : m_features)
        {
            if (feature)
            {
                feature->Shutdown();
                delete feature;
            }
        }
        m_features.clear();
        LOG_INFO("Feature manager stopped");
    }

    void Manager::TickUpdateLoop(IFeature* feature)
    {
        auto& ft = m_threads[feature];
        while (is_running())
        {
            {
                std::unique_lock<std::mutex> lock(ft.mtx);
                ft.cv.wait(lock, [&] {
                    return !is_running() || feature->GetEnabled();
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
        for (IFeature* feature : m_features)
        {
            if (feature && feature->GetEnabled())
                feature->OnDraw();
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

        for (IFeature* feature : m_features)
        {
            if (!feature)
                continue;

            bool& enabled = feature->GetEnabled();
            bool was_enabled = enabled;
            ImGui::Checkbox(feature->GetName(), &enabled);

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