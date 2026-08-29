//
// Created by superhuap on 2026/8/25.
//

#include "core/FeatureManager.h"

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

        m_updateThread = std::thread(&Manager::TickUpdateLoop, this);
        LOG_INFO("Feature manager started ({} feature(s))", m_features.size());
    }

    void Manager::Stop()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_running.exchange(false))
                return;
        }

        if (m_updateThread.joinable())
            m_updateThread.join();

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

    void Manager::TickUpdateLoop()
    {
        while (m_running.load())
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (IFeature* feature : m_features)
            {
                if (feature && feature->GetEnabled())
                    feature->OnUpdate();
            }
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
            ImGui::Checkbox(feature->GetName(), &enabled);

            if (enabled)
            {
                ImGui::Indent();
                feature->DrawOptions();
                ImGui::Unindent();
            }
        }
    }

} // namespace Feature