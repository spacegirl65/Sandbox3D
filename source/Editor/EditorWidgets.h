// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include <imgui.h>
#include <string_view>

namespace Sandbox3D::Engine
{
    class Base;
}

namespace Sandbox3D::Editor
{
    inline constexpr ImU32 PanelIconColour = IM_COL32(205, 208, 214, 255);

    enum class PanelIcon
    {
        SceneHierarchy,
        ObjectInspector,
        ResourceBrowser,
        Terminal
    };

    enum class SceneObjectIcon
    {
        Scene,
        Base,
        Body,
        Camera,
        Light,
        Terrain
    };

    [[nodiscard]] SceneObjectIcon ResolveSceneObjectIcon(const Engine::Base& object);

    void DrawPanelIcon(ImDrawList* dl, PanelIcon icon, const ImVec2& center, float size, ImU32 col);
    void DrawSceneObjectIcon(ImDrawList* dl, SceneObjectIcon icon, const ImVec2& center, float size, ImU32 col);
    void DrawPowerIcon(ImDrawList* dl, const ImVec2& center, float size, ImU32 col);
    void DrawEyeIcon(ImDrawList* dl, const ImVec2& center, float size, ImU32 col, bool visible);
    void DrawCurrentWindowTabIcon(PanelIcon icon);

    bool CompactMenuItem(const char* label, const char* shortcut = nullptr, bool enabled = true);
    [[nodiscard]] bool ContainsCaseInsensitive(std::string_view haystack, std::string_view needle);
}
