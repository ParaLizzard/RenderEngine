#pragma once
#include <string_view>
#include <format>
#include <source_location>

namespace Engine {
    enum class LogLevel {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Fatal
    };

    class Logger {
    public:
        static void Log(LogLevel level,
                        std::string_view category,
                        std::string_view message,
                        const std::source_location& loc = std::source_location::current());

        template<typename... Args>
        static void FormatLog(LogLevel level,
                              std::string_view category,
                              const std::source_location& loc,
                              std::format_string<Args...> fmt,
                              Args&&... args) {
            std::string msg = std::format(fmt, std::forward<Args>(args)...);
            Log(level, category, msg, loc);
        }
    };
}

#ifndef NDEBUG
    #define LOG_TRACE(Category, Fmt, ...) ::Engine::Logger::FormatLog(::Engine::LogLevel::Trace, Category, std::source_location::current(), Fmt __VA_OPT__(,) __VA_ARGS__)
    #define LOG_DEBUG(Category, Fmt, ...) ::Engine::Logger::FormatLog(::Engine::LogLevel::Debug, Category, std::source_location::current(), Fmt __VA_OPT__(,) __VA_ARGS__)
#else
    #define LOG_TRACE(Category, Fmt, ...) do { (void)sizeof(Category); } while(false)
    #define LOG_DEBUG(Category, Fmt, ...) do { (void)sizeof(Category); } while(false)
#endif
#define LOG_INFO(Category, Fmt, ...)  ::Engine::Logger::FormatLog(::Engine::LogLevel::Info,  Category, std::source_location::current(), Fmt __VA_OPT__(,) __VA_ARGS__)
#define LOG_WARN(Category, Fmt, ...)  ::Engine::Logger::FormatLog(::Engine::LogLevel::Warn,  Category, std::source_location::current(), Fmt __VA_OPT__(,) __VA_ARGS__)
#define LOG_ERROR(Category, Fmt, ...) ::Engine::Logger::FormatLog(::Engine::LogLevel::Error, Category, std::source_location::current(), Fmt __VA_OPT__(,) __VA_ARGS__)
#define LOG_FATAL(Category, Fmt, ...) ::Engine::Logger::FormatLog(::Engine::LogLevel::Fatal, Category, std::source_location::current(), Fmt __VA_OPT__(,) __VA_ARGS__)
