// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "ResourceBrowserPanel.h"
#include "EditorWidgets.h"

#include <imgui.h>

namespace Sandbox3D::Editor
{
    namespace
    {
        constexpr const char* ResourceBrowserWindowLabel = "      Resource Browser###Resource Browser";
    }

    void ResourceBrowserPanel::Draw(bool& isOpen)
    {
        if (ImGui::Begin(ResourceBrowserWindowLabel, &isOpen))
        {
            ImGui::TextDisabled("resources/");
            ImGui::BulletText("entities");
            ImGui::BulletText("environment");
            ImGui::BulletText("shared");
        }
        DrawCurrentWindowTabIcon(PanelIcon::ResourceBrowser);
        ImGui::End();
    }
}
