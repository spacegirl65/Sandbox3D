// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include <string>
#include <vector>

struct ImFont;

namespace Sandbox3D::Editor
{
    // Renders the Terminal panel and stores captured log entries and command input state
    class TerminalPanel final
    {
    public:
        void Draw(bool& isOpen, ImFont* monoFont);
        void AppendLog(std::string entry);

    private:
        std::vector<std::string> m_lines;
        std::string              m_inputBuffer;
    };
}
