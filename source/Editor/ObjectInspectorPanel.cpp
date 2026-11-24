// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "ObjectInspectorPanel.h"
#include "EditorWidgets.h"
#include "Engine/Base.h"
#include "Engine/Scene.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstring>

namespace Sandbox3D::Editor
{
    namespace
    {
        constexpr const char* ObjectInspectorWindowLabel = "      Object Inspector###Object Inspector";
    }

    void ObjectInspectorPanel::Draw(EditorContext& context, bool& isOpen)
    {
        if (ImGui::Begin(ObjectInspectorWindowLabel, &isOpen))
        {
            std::shared_ptr<Engine::Base> selected;
            if (context.scene && context.selectedObjectId != 0)
            {
                selected = context.scene->FindObject(context.selectedObjectId);
            }

            if (!selected)
            {
                ImGui::TextDisabled("Select an object in the hierarchy to inspect its properties.");
            }
            else
            {
                ImGui::PushID(static_cast<int>(selected->GetId()));

                // Header: class icon + editable name
                const float iconSize = ImGui::GetFrameHeight() * 0.85f;
                const ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                const ImVec2 iconCenter(cursorPos.x + iconSize * 0.5f, cursorPos.y + ImGui::GetFrameHeight() * 0.5f);
                DrawSceneObjectIcon(
                    ImGui::GetWindowDrawList(),
                    ResolveSceneObjectIcon(*selected),
                    iconCenter,
                    iconSize,
                    PanelIconColour
                );
                ImGui::Dummy(ImVec2(iconSize, ImGui::GetFrameHeight()));
                ImGui::SameLine();

                std::array<char, 128> nameBuffer{};
                const std::string& currentName = selected->GetName();
                const size_t copyLen = std::min(currentName.size(), nameBuffer.size() - 1);
                std::memcpy(nameBuffer.data(), currentName.data(), copyLen);

                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if (ImGui::InputText("##InspectorName", nameBuffer.data(), nameBuffer.size()) && nameBuffer[0] != '\0')
                {
                    selected->SetName(nameBuffer.data());
                }

                // Read-only ID, Type, and optional Parent metadata
                const std::string_view typeName = selected->GetTypeName();
                ImGui::TextDisabled("ID: %u  |  Type: %.*s", selected->GetId(), static_cast<int>(typeName.size()), typeName.data());
                if (const auto parent = selected->GetParent())
                {
                    const char* parentName = parent->GetName().empty() ? "<Unnamed>" : parent->GetName().c_str();
                    ImGui::TextDisabled("Parent: %s", parentName);
                }

                // Active / Visible toggles
                bool isActive  = selected->IsActive();
                bool isVisible = selected->IsVisible();
                if (ImGui::Checkbox("Active", &isActive))
                {
                    selected->SetActive(isActive);
                }
                ImGui::SameLine();
                if (ImGui::Checkbox("Visible", &isVisible))
                {
                    selected->SetVisible(isVisible);
                }

                ImGui::Separator();

                // Transform section (64-bit Vec3D position)
                if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    const Maths::Vec3D currentPos = selected->GetPosition();
                    double position[3] = { currentPos.x, currentPos.y, currentPos.z };
                    if (ImGui::DragScalarN("Position", ImGuiDataType_Double, position, 3, 0.1f, nullptr, nullptr, "%.3f"))
                    {
                        selected->SetPosition(Maths::Vec3D(position[0], position[1], position[2]));
                    }
                }

                ImGui::PopID();
            }
        }
        DrawCurrentWindowTabIcon(PanelIcon::ObjectInspector);
        ImGui::End();
    }
}
