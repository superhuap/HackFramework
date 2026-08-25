//
// Created by superhuap on 2026/8/25.
//

#ifndef HACKFRAMEWORK_LOGGER_H
#define HACKFRAMEWORK_LOGGER_H

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <fmt/format.h>

namespace Logger
{

    enum class Level
    {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Critical
    };

    void Init();
    void Shutdown();

    template<typename... Args>
    void Log(Level level, fmt::format_string<Args...> fmt, Args&&... args)
    {
        auto logger = spdlog::default_logger_raw();
        if (!logger) return;

        auto message = fmt::format(fmt, std::forward<Args>(args)...);

        switch (level)
        {
            case Level::Trace:    logger->trace(message); break;
            case Level::Debug:    logger->debug(message); break;
            case Level::Info:     logger->info(message);  break;
            case Level::Warn:     logger->warn(message);  break;
            case Level::Error:    logger->error(message); break;
            case Level::Critical: logger->critical(message); break;
        }
    }

    template<typename... Args>
    void Trace(fmt::format_string<Args...> fmt, Args&&... args)
    {
        Log(Level::Trace, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Debug(fmt::format_string<Args...> fmt, Args&&... args)
    {
        Log(Level::Debug, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Info(fmt::format_string<Args...> fmt, Args&&... args)
    {
        Log(Level::Info, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Warn(fmt::format_string<Args...> fmt, Args&&... args)
    {
        Log(Level::Warn, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Error(fmt::format_string<Args...> fmt, Args&&... args)
    {
        Log(Level::Error, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Critical(fmt::format_string<Args...> fmt, Args&&... args)
    {
        Log(Level::Critical, fmt, std::forward<Args>(args)...);
    }

} // namespace Logger

#define LOG_TRACE(...)    ::Logger::Trace(__VA_ARGS__)
#define LOG_DEBUG(...)    ::Logger::Debug(__VA_ARGS__)
#define LOG_INFO(...)     ::Logger::Info(__VA_ARGS__)
#define LOG_WARN(...)     ::Logger::Warn(__VA_ARGS__)
#define LOG_ERROR(...)    ::Logger::Error(__VA_ARGS__)
#define LOG_CRITICAL(...) ::Logger::Critical(__VA_ARGS__)

#endif //HACKFRAMEWORK_LOGGER_H
