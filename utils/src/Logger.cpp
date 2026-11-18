//
// Created by superhuap on 2026/11/17.
//

#include "utils/Logger.h"

#ifdef DEBUG_BUILD
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Logger {

    namespace {
        bool g_initialized = false;
    }

    void Init() {
        if (g_initialized) return;
        g_initialized = true;

#ifndef DEBUG_BUILD
        spdlog::set_level(spdlog::level::off);
        return;
#else
        if (AllocConsole()) {
            SetConsoleOutputCP(CP_UTF8);
            FILE* fDummy;
            freopen_s(&fDummy, "CONOUT$", "w", stdout);
            freopen_s(&fDummy, "CONOUT$", "w", stderr);
            freopen_s(&fDummy, "conin$", "r", stdin);

            HWND hConsole = GetConsoleWindow();
            if (hConsole) {
                SetConsoleTitleA("Debug Logger Console");
                ShowWindow(hConsole, SW_SHOW);
            }

            std::setvbuf(stdout, nullptr, _IONBF, 0);
        }

        spdlog::init_thread_pool(8192, 1);

        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_st>();
        console_sink->set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] [%t] [%l] %v%$");

        auto logger = std::make_shared<spdlog::async_logger>(
            "Logger",
            console_sink,
            spdlog::thread_pool(),
            spdlog::async_overflow_policy::overrun_oldest
        );

        logger->set_level(spdlog::level::debug);
        logger->flush_on(spdlog::level::err);

        spdlog::set_default_logger(logger);
#endif
    }

    void Shutdown() {
        if (!g_initialized) return;
        g_initialized = false;

        spdlog::shutdown();

#ifdef DEBUG_BUILD
        FILE* fDummy;
        freopen_s(&fDummy, "NUL", "w", stdout);
        freopen_s(&fDummy, "NUL", "w", stderr);
        freopen_s(&fDummy, "NUL", "r", stdin);
        FreeConsole();
#endif
    }
}
