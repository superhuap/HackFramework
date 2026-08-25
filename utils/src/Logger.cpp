//
// Created by superhuap on 2026/8/25.
//

#include "utils/Logger.h"
#include <Windows.h>
#include <cstdio>
#include "config.h"

namespace Logger {

    void Init() {
        // 分配控制台
        if (AllocConsole()) {
            SetConsoleOutputCP(CP_UTF8);
            FILE* fDummy;
            freopen_s(&fDummy, "CONOUT$", "w", stdout);
            freopen_s(&fDummy, "CONOUT$", "w", stderr);

            HWND hConsole = GetConsoleWindow();
#ifdef ENABLE_LOGGING
            SetConsoleTitleA(fmt::format("{} - Debug Logger Console", PROJECT_NAME).c_str());
            ShowWindow(hConsole, SW_SHOW);
#else
            ShowWindow(hConsole, SW_HIDE);
#endif
            std::setvbuf(stdout, nullptr, _IONBF, 0);
        }

        // 初始化 spdlog 线程池
        spdlog::init_thread_pool(8192, 1);


        // 创建控制台 sink
#ifdef ENABLE_LOGGING_COLOR
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_st>();
#else
        auto console_sink = std::make_shared<spdlog::sinks::stdout_sink_st>();
#endif
        console_sink->set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v%$");

        // 创建异步 logger
        auto logger = std::make_shared<spdlog::async_logger>(
            PROJECT_NAME,
            console_sink,
            spdlog::thread_pool(),
            spdlog::async_overflow_policy::overrun_oldest
        );

        logger->set_level(spdlog::level::debug);
        logger->flush_on(spdlog::level::err);

        spdlog::set_default_logger(logger);
    }

    void Shutdown() {
        spdlog::shutdown();

        // 关闭控制台
        FILE* fDummy;
        freopen_s(&fDummy, "NUL", "w", stdout);
        freopen_s(&fDummy, "NUL", "w", stderr);
        FreeConsole();
    }

} // namespace Logger
