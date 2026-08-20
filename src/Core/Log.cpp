#include "Log.h"

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#endif

namespace Engine
{
    void Logger::Log(LogLevel level,
        std::string_view category,
        std::string_view message,
        const std::source_location &loc)
    {
        std::string line;

        switch (level) {
        case LogLevel::Trace: {
            line = std::format("\033[90m[TRACE] [{}] {}\033[0m\n", category, message);
            std::fwrite(line.data(), 1, line.size(), stdout);
            break;
        }
        case LogLevel::Debug: {
            line = std::format("\033[36m[DEBUG] [{}] {}\033[0m\n", category, message);
            std::fwrite(line.data(), 1, line.size(), stdout);
            break;
        }
        case LogLevel::Info: {
           line = std::format("\033[97m[INFO] [{}] {}\033[0m\n", category, message);
            std::fwrite(line.data(), 1, line.size(), stdout);
            break;
        }
        case LogLevel::Warn: {
            line = std::format("\033[33;1m[WARN] [{}] {} in function {}\033[0m\n", category, message, loc.function_name());
            std::fwrite(line.data(), 1, line.size(), stdout);
            break;
        }
        case LogLevel::Error: {
            line = std::format("\033[31;1m[ERROR] [{}] {} in {} on line {} \033[0m\n", category, message, loc.file_name(), loc.line());
            std::fwrite(line.data(), 1, line.size(), stderr);
            std::fflush(stdout);
            std::fflush(stderr);
            break;
        }
        case LogLevel::Fatal: {
            line = std::format("\033[41;1;97m[FATAL] [{}] {} in {} on line {} \033[0m\n", category, message, loc.file_name(), loc.line());
            std::fwrite(line.data(), 1, line.size(), stderr);
            std::fflush(stdout);
            std::fflush(stderr);
            break;
        }
        default: {
            line = std::format("\033[0m [{}] {}\033[0m\n", category, message);
            std::fwrite(line.data(), 1, line.size(), stdout);
            break;
        }
        }

        #if defined(_WIN32)
            OutputDebugStringA(line.c_str());
        #endif
    }
} // Engine