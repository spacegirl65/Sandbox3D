// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include <imgui.h>
#include <span>
#include <string_view>

struct ImFont;

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

    struct SearchableOption
    {
        const char* label{ nullptr };
        bool        selected{ false };
    };

    bool CompactMenuItem(const char* label, const char* shortcut = nullptr, bool enabled = true);
    [[nodiscard]] bool ContainsCaseInsensitive(std::string_view haystack, std::string_view needle);

    // Renders a compact searchable list inside an open menu or popup; returns clicked option index or -1
    int DrawSearchablePickerList(
        const char* id,
        std::span<const SearchableOption> options,
        int maxVisibleItems = 4,
        ImFont* italicFont = nullptr,
        bool closePopupOnSelect = true
    );

    // Renders a single-select combo box backed by the shared searchable picker
    int DrawSearchableCombo(
        const char* id,
        const char* preview,
        std::span<const SearchableOption> options,
        int maxVisibleItems = 4,
        ImFont* italicFont = nullptr
    );

    // Renders a multi-select checklist combo box backed by the shared searchable picker; returns toggled option index or -1
    int DrawSearchableChecklistCombo(
        const char* id,
        const char* preview,
        std::span<const SearchableOption> options,
        int maxVisibleItems = 4,
        ImFont* italicFont = nullptr
    );
}
