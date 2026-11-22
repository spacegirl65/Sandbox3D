// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "EditorLogger.h"
#include "EditorWindow.h"

#include <string>

namespace Sandbox3D::Editor
{
    EditorLogger::EditorLogger(EditorWindow* editorWindow) noexcept
        : m_editorWindow(editorWindow)
    {
    }

    void EditorLogger::Log(Core::LogLevel level, std::string_view message)
    {
        if (!m_editorWindow)
        {
            return;
        }

        std::string line(message);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
        {
            line.pop_back();
        }

        if (line.empty())
        {
            return;
        }

        std::scoped_lock lock(m_outputMutex);
        switch (level)
        {
        case Core::LogLevel::Warning:
            m_editorWindow->AppendTerminalLog("[Warning] " + line);
            break;
        case Core::LogLevel::Error:
            m_editorWindow->AppendTerminalLog("[Error] " + line);
            break;
        case Core::LogLevel::Info:
        default:
            m_editorWindow->AppendTerminalLog(std::move(line));
            break;
        }
    }
}
