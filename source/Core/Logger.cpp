// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Logger.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>

namespace Sandbox3D::Core
{
    void Logger::Info(std::string_view message)
    {
        Log(LogLevel::Info, message);
    }

    void Logger::Warning(std::string_view message)
    {
        Log(LogLevel::Warning, message);
    }

    void Logger::Error(std::string_view message)
    {
        Log(LogLevel::Error, message);
    }

    void Logger::Log(LogLevel level, std::wstring_view message)
    {
        Log(level, WideToUtf8(message));
    }

    void Logger::Info(std::wstring_view message)
    {
        Log(LogLevel::Info, message);
    }

    void Logger::Warning(std::wstring_view message)
    {
        Log(LogLevel::Warning, message);
    }

    void Logger::Error(std::wstring_view message)
    {
        Log(LogLevel::Error, message);
    }

    void Logger::RegisterSink(Logger* sink)
    {
        if (!sink)
        {
            return;
        }

        std::scoped_lock lock(s_sinkMutex);
        if (std::find(s_sinks.begin(), s_sinks.end(), sink) == s_sinks.end())
        {
            s_sinks.push_back(sink);
        }
    }

    void Logger::UnregisterSink(Logger* sink) noexcept
    {
        if (!sink)
        {
            return;
        }

        std::scoped_lock lock(s_sinkMutex);
        std::erase(s_sinks, sink);
    }

    void Logger::LogGlobal(LogLevel level, std::string_view message)
    {
        std::scoped_lock lock(s_sinkMutex);
        for (Logger* sink : s_sinks)
        {
            if (sink)
            {
                sink->Log(level, message);
            }
        }
    }

    void Logger::LogGlobal(LogLevel level, std::wstring_view message)
    {
        LogGlobal(level, WideToUtf8(message));
    }

    void Logger::LogInfo(std::string_view message)
    {
        LogGlobal(LogLevel::Info, message);
    }

    void Logger::LogInfo(std::wstring_view message)
    {
        LogGlobal(LogLevel::Info, message);
    }

    void Logger::LogWarning(std::string_view message)
    {
        LogGlobal(LogLevel::Warning, message);
    }

    void Logger::LogWarning(std::wstring_view message)
    {
        LogGlobal(LogLevel::Warning, message);
    }

    void Logger::LogError(std::string_view message)
    {
        LogGlobal(LogLevel::Error, message);
    }

    void Logger::LogError(std::wstring_view message)
    {
        LogGlobal(LogLevel::Error, message);
    }

    std::string Logger::WideToUtf8(std::wstring_view wideText)
    {
        if (wideText.empty())
        {
            return {};
        }

        const int requiredBytes = WideCharToMultiByte(
            CP_UTF8,
            0,
            wideText.data(),
            static_cast<int>(wideText.size()),
            nullptr,
            0,
            nullptr,
            nullptr
        );
        if (requiredBytes <= 0)
        {
            return {};
        }

        std::string result(static_cast<size_t>(requiredBytes), '\0');
        WideCharToMultiByte(
            CP_UTF8,
            0,
            wideText.data(),
            static_cast<int>(wideText.size()),
            result.data(),
            requiredBytes,
            nullptr,
            nullptr
        );
        return result;
    }
}
