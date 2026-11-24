// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "TerminalPanel.h"
#include "EditorWidgets.h"
#include "Core/Logger.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstring>

namespace Sandbox3D::Editor
{
    namespace
    {
        constexpr const char* TerminalWindowLabel = "      Terminal###Terminal";
    }

    void TerminalPanel::AppendLog(std::string entry)
    {
        m_lines.push_back(std::move(entry));
    }

    void TerminalPanel::Draw(bool& isOpen, ImFont* monoFont)
    {
        if (ImGui::Begin(TerminalWindowLabel, &isOpen))
        {
            if (monoFont)
            {
                ImGui::PushFont(monoFont);
            }

            const float inputFooterReserve = ImGui::GetFrameHeightWithSpacing() + 6.0f;
            if (ImGui::BeginChild("TerminalScrollRegion", ImVec2(0.0f, -inputFooterReserve), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
            {
                for (const std::string& line : m_lines)
                {
                    if (line.rfind("[Error]", 0) == 0)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.933f, 0.337f, 0.361f, 1.00f));
                        ImGui::TextUnformatted(line.c_str());
                        ImGui::PopStyleColor();
                    }
                    else if (line.rfind("[Warning]", 0) == 0)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.980f, 0.722f, 0.227f, 1.00f));
                        ImGui::TextUnformatted(line.c_str());
                        ImGui::PopStyleColor();
                    }
                    else
                    {
                        ImGui::TextUnformatted(line.c_str());
                    }
                }
                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                {
                    ImGui::SetScrollHereY(1.0f);
                }
            }
            ImGui::EndChild();

            ImGui::Separator();
            std::array<char, 256> inputBuffer{};
            const size_t copyLen = std::min(m_inputBuffer.size(), inputBuffer.size() - 1);
            std::memcpy(inputBuffer.data(), m_inputBuffer.data(), copyLen);

            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputTextWithHint("##TerminalInput", "Enter command...", inputBuffer.data(), inputBuffer.size(), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                std::string command(inputBuffer.data());
                if (!command.empty())
                {
                    Core::Logger::LogInfo("> " + command);
                    m_inputBuffer.clear();
                }
            }
            else
            {
                m_inputBuffer = inputBuffer.data();
            }

            if (monoFont)
            {
                ImGui::PopFont();
            }
        }
        DrawCurrentWindowTabIcon(PanelIcon::Terminal);
        ImGui::End();
    }
}
