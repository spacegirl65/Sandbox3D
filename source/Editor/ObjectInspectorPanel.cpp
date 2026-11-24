// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "ObjectInspectorPanel.h"
#include "EditorWidgets.h"

#include <imgui.h>

namespace Sandbox3D::Editor
{
    namespace
    {
        constexpr const char* ObjectInspectorWindowLabel = "      Object Inspector###Object Inspector";
    }

    void ObjectInspectorPanel::Draw([[maybe_unused]] EditorContext& context, bool& isOpen)
    {
        if (ImGui::Begin(ObjectInspectorWindowLabel, &isOpen))
        {
            ImGui::TextDisabled("Select an object in the hierarchy to inspect its properties.");
        }
        DrawCurrentWindowTabIcon(PanelIcon::ObjectInspector);
        ImGui::End();
    }
}
