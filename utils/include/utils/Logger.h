//
// Created by superhuap on 2026/11/17.
//

#ifndef HACKFRAMEWORK_LOGGER_H
#define HACKFRAMEWORK_LOGGER_H

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <fmt/format.h>

namespace Logger {

    enum class Level{
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Critical
    };

    constexpr spdlog::level::level_enum ToSpdLevel(Level level) {
        switch (level) {
            case Level::Trace:    return spdlog::level::trace;
            case Level::Debug:    return spdlog::level::debug;
            case Level::Info:     return spdlog::level::info;
            case Level::Warn:     return spdlog::level::warn;
            case Level::Error:    return spdlog::level::err;
            case Level::Critical: return spdlog::level::critical;
        }
        return spdlog::level::off;
    }

    void Init();
    void Shutdown();

    template<typename... Args>
    void Log(Level level, fmt::format_string<Args...> fmt, Args&&... args){
        auto logger = spdlog::default_logger_raw();
        if (!logger) return;

        logger->log(ToSpdLevel(level), fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Trace(fmt::format_string<Args...> fmt, Args&&... args){
        Log(Level::Trace, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Debug(fmt::format_string<Args...> fmt, Args&&... args){
        Log(Level::Debug, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Info(fmt::format_string<Args...> fmt, Args&&... args){
        Log(Level::Info, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Warn(fmt::format_string<Args...> fmt, Args&&... args){
        Log(Level::Warn, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Error(fmt::format_string<Args...> fmt, Args&&... args){
        Log(Level::Error, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Critical(fmt::format_string<Args...> fmt, Args&&... args){
        Log(Level::Critical, fmt, std::forward<Args>(args)...);
    }

}

#endif //HACKFRAMEWORK_LOGGER_H
