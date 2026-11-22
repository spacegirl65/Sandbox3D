// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace Sandbox3D::Core
{
    // Severity classification for engine and editor log entries
    enum class LogLevel
    {
        Info,
        Warning,
        Error
    };

    // Abstract base class for engine logging sinks, supporting both direct and broadcast dispatch
    class Logger
    {
    public:
        Logger() = default;
        virtual ~Logger() = default;

        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;
        Logger(Logger&&) noexcept = delete;
        Logger& operator=(Logger&&) noexcept = delete;

        virtual void Log(LogLevel level, std::string_view message) = 0;

        void Info(std::string_view message);
        void Warning(std::string_view message);
        void Error(std::string_view message);

        void Log(LogLevel level, std::wstring_view message);
        void Info(std::wstring_view message);
        void Warning(std::wstring_view message);
        void Error(std::wstring_view message);

        static void RegisterSink(Logger* sink);
        static void UnregisterSink(Logger* sink) noexcept;

        static void LogGlobal(LogLevel level, std::string_view message);
        static void LogGlobal(LogLevel level, std::wstring_view message);
        static void LogInfo(std::string_view message);
        static void LogInfo(std::wstring_view message);
        static void LogWarning(std::string_view message);
        static void LogWarning(std::wstring_view message);
        static void LogError(std::string_view message);
        static void LogError(std::wstring_view message);

    protected:
        [[nodiscard]] static std::string WideToUtf8(std::wstring_view wideText);

    private:
        inline static std::mutex           s_sinkMutex{};
        inline static std::vector<Logger*> s_sinks{};
    };
}
