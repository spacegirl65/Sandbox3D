// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "ConsoleLogger.h"

#include <iostream>

namespace Sandbox3D::Core
{
    void ConsoleLogger::Log(LogLevel level, std::string_view message)
    {
        std::scoped_lock lock(m_outputMutex);

        std::ostream& targetStream = (level == LogLevel::Error) ? std::cerr : std::cout;
        targetStream << message;
        if (message.empty() || message.back() != '\n')
        {
            targetStream << '\n';
        }
    }
}
