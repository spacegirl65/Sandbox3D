// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Logger.h"

#include <mutex>

namespace Sandbox3D::Core
{
    // Logger sink writing formatted diagnostic entries to the native Win32 console terminal
    class ConsoleLogger final : public Logger
    {
    public:
        ConsoleLogger() = default;
        ~ConsoleLogger() override = default;

        ConsoleLogger(const ConsoleLogger&) = delete;
        ConsoleLogger& operator=(const ConsoleLogger&) = delete;
        ConsoleLogger(ConsoleLogger&&) noexcept = delete;
        ConsoleLogger& operator=(ConsoleLogger&&) noexcept = delete;

        void Log(LogLevel level, std::string_view message) override;

    private:
        std::mutex m_outputMutex;
    };
}
