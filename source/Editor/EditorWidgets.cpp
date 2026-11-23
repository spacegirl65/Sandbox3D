// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "EditorWidgets.h"

#include "Engine/Base.h"
#include "Engine/Body.h"
#include "Engine/Camera.h"
#include "Engine/Light.h"
#include "Engine/Terrain/TerrainObject.h"

#include <imgui_internal.h>

#include <algorithm>
#include <cctype>
#include <cmath>

namespace Sandbox3D::Editor
{
    SceneObjectIcon ResolveSceneObjectIcon(const Engine::Base& object)
    {
        if (dynamic_cast<const Engine::Camera*>(&object) != nullptr)
        {
            return SceneObjectIcon::Camera;
        }
        if (dynamic_cast<const Engine::Light*>(&object) != nullptr)
        {
            return SceneObjectIcon::Light;
        }
        if (dynamic_cast<const Engine::Terrain::TerrainObject*>(&object) != nullptr)
        {
            return SceneObjectIcon::Terrain;
        }
        if (dynamic_cast<const Engine::Body*>(&object) != nullptr)
        {
            return SceneObjectIcon::Body;
        }
        return SceneObjectIcon::Base;
    }

    void DrawPanelIcon(ImDrawList* dl, PanelIcon icon, const ImVec2& center, float size, ImU32 col)
    {
        const float half   = size * 0.5f;
        const float x0     = center.x - half;
        const float y0     = center.y - half;
        const float x1     = center.x + half;
        const float stroke = std::max(1.2f, size * 0.11f);

        switch (icon)
        {
        case PanelIcon::SceneHierarchy:
        {
            const float trunkX = x0 + size * 0.18f;
            const float yTop   = y0 + size * 0.20f;
            const float yMid   = y0 + size * 0.52f;
            const float yBot   = y0 + size * 0.84f;
            const float boxSz  = std::max(2.0f, size * 0.22f);

            dl->AddRectFilled(ImVec2(trunkX - boxSz * 0.5f, yTop - boxSz * 0.5f), ImVec2(trunkX + boxSz * 0.5f, yTop + boxSz * 0.5f), col, 1.0f);
            dl->AddLine(ImVec2(trunkX, yTop + boxSz * 0.5f), ImVec2(trunkX, yBot), col, stroke);
            dl->AddLine(ImVec2(trunkX, yMid), ImVec2(x1 - boxSz, yMid), col, stroke);
            dl->AddLine(ImVec2(trunkX, yBot), ImVec2(x1 - boxSz, yBot), col, stroke);
            dl->AddRectFilled(ImVec2(x1 - boxSz, yMid - boxSz * 0.5f), ImVec2(x1, yMid + boxSz * 0.5f), col, 1.0f);
            dl->AddRectFilled(ImVec2(x1 - boxSz, yBot - boxSz * 0.5f), ImVec2(x1, yBot + boxSz * 0.5f), col, 1.0f);
            break;
        }
        case PanelIcon::ObjectInspector:
        {
            const float yRows[3] = { y0 + size * 0.22f, center.y, y0 + size * 0.78f };
            const float knobX[3] = { x0 + size * 0.34f, x0 + size * 0.70f, x0 + size * 0.44f };
            const float knobR    = std::max(1.6f, size * 0.12f);

            for (int i = 0; i < 3; ++i)
            {
                dl->AddLine(ImVec2(x0 + size * 0.08f, yRows[i]), ImVec2(x1 - size * 0.08f, yRows[i]), col, stroke);
                dl->AddCircleFilled(ImVec2(knobX[i], yRows[i]), knobR, col);
            }
            break;
        }
        case PanelIcon::ResourceBrowser:
        {
            const float tabRight = x0 + size * 0.46f;
            const float tabY     = y0 + size * 0.24f;
            const float bodyTop  = y0 + size * 0.36f;
            const float bodyBot  = y0 + size * 0.84f;

            dl->AddRectFilled(ImVec2(x0 + size * 0.06f, tabY), ImVec2(tabRight, bodyTop + 1.5f), col, 1.5f, ImDrawFlags_RoundCornersTop);
            dl->AddRect(ImVec2(x0 + size * 0.06f, bodyTop), ImVec2(x1 - size * 0.06f, bodyBot), col, 1.5f, 0, stroke);
            dl->AddLine(ImVec2(x0 + size * 0.06f, bodyTop + size * 0.14f), ImVec2(x1 - size * 0.06f, bodyTop + size * 0.14f), col, stroke);
            break;
        }
        case PanelIcon::Terminal:
        {
            const float chevLeft  = x0 + size * 0.14f;
            const float chevRight = x0 + size * 0.46f;
            const float chevTop   = y0 + size * 0.24f;
            const float chevBot   = y0 + size * 0.72f;
            const float chevMidY  = (chevTop + chevBot) * 0.5f;

            dl->AddLine(ImVec2(chevLeft, chevTop), ImVec2(chevRight, chevMidY), col, stroke);
            dl->AddLine(ImVec2(chevRight, chevMidY), ImVec2(chevLeft, chevBot), col, stroke);
            dl->AddLine(ImVec2(x0 + size * 0.52f, chevBot), ImVec2(x1 - size * 0.10f, chevBot), col, stroke);
            break;
        }
        }
    }

    void DrawSceneObjectIcon(ImDrawList* dl, SceneObjectIcon icon, const ImVec2& center, float size, ImU32 col)
    {
        const float half   = size * 0.5f;
        const float x0     = center.x - half;
        const float y0     = center.y - half;
        const float stroke = std::max(1.2f, size * 0.11f);

        switch (icon)
        {
        case SceneObjectIcon::Scene:
        {
            const float r = size * 0.42f;
            dl->AddCircle(center, r, col, 16, stroke);
            dl->AddLine(ImVec2(center.x - r, center.y), ImVec2(center.x + r, center.y), col, stroke);
            dl->AddBezierCubic(
                ImVec2(center.x, center.y - r),
                ImVec2(center.x - r * 0.55f, center.y - r * 0.45f),
                ImVec2(center.x - r * 0.55f, center.y + r * 0.45f),
                ImVec2(center.x, center.y + r),
                col, stroke, 8
            );
            dl->AddBezierCubic(
                ImVec2(center.x, center.y - r),
                ImVec2(center.x + r * 0.55f, center.y - r * 0.45f),
                ImVec2(center.x + r * 0.55f, center.y + r * 0.45f),
                ImVec2(center.x, center.y + r),
                col, stroke, 8
            );
            break;
        }
        case SceneObjectIcon::Base:
        {
            const ImVec2 diamond[4] =
            {
                ImVec2(center.x, y0 + size * 0.12f),
                ImVec2(x0 + size * 0.88f, center.y),
                ImVec2(center.x, y0 + size * 0.88f),
                ImVec2(x0 + size * 0.12f, center.y)
            };
            dl->AddPolyline(diamond, 4, col, ImDrawFlags_Closed, stroke);
            dl->AddCircleFilled(center, std::max(1.4f, size * 0.11f), col);
            break;
        }
        case SceneObjectIcon::Body:
        {
            const ImVec2 top(center.x, y0 + size * 0.08f);
            const ImVec2 upperRight(x0 + size * 0.88f, y0 + size * 0.28f);
            const ImVec2 lowerRight(x0 + size * 0.88f, y0 + size * 0.72f);
            const ImVec2 bottom(center.x, y0 + size * 0.92f);
            const ImVec2 lowerLeft(x0 + size * 0.12f, y0 + size * 0.72f);
            const ImVec2 upperLeft(x0 + size * 0.12f, y0 + size * 0.28f);
            const ImVec2 cubeCenter(center.x, y0 + size * 0.50f);

            const ImVec2 outline[6] = { top, upperRight, lowerRight, bottom, lowerLeft, upperLeft };
            dl->AddPolyline(outline, 6, col, ImDrawFlags_Closed, stroke);
            dl->AddLine(cubeCenter, upperLeft, col, stroke);
            dl->AddLine(cubeCenter, upperRight, col, stroke);
            dl->AddLine(cubeCenter, bottom, col, stroke);
            break;
        }
        case SceneObjectIcon::Camera:
        {
            const float bodyLeft   = x0 + size * 0.08f;
            const float bodyRight  = x0 + size * 0.66f;
            const float bodyTop    = y0 + size * 0.24f;
            const float bodyBottom = y0 + size * 0.76f;

            dl->AddRectFilled(ImVec2(bodyLeft, bodyTop), ImVec2(bodyRight, bodyBottom), col, 1.5f);
            dl->AddQuadFilled(
                ImVec2(x0 + size * 0.64f, y0 + size * 0.38f),
                ImVec2(x0 + size * 0.94f, y0 + size * 0.22f),
                ImVec2(x0 + size * 0.94f, y0 + size * 0.78f),
                ImVec2(x0 + size * 0.64f, y0 + size * 0.62f),
                col
            );
            break;
        }
        case SceneObjectIcon::Light:
        {
            constexpr float pi      = 3.14159265358979323846f;
            const float innerRadius = size * 0.20f;
            const float rayStart    = size * 0.31f;
            const float rayEnd      = size * 0.47f;

            dl->AddCircleFilled(center, innerRadius, col);
            for (int i = 0; i < 8; ++i)
            {
                const float angle = static_cast<float>(i) * (pi * 0.25f);
                const float cosA  = std::cos(angle);
                const float sinA  = std::sin(angle);
                dl->AddLine(
                    ImVec2(center.x + cosA * rayStart, center.y + sinA * rayStart),
                    ImVec2(center.x + cosA * rayEnd, center.y + sinA * rayEnd),
                    col,
                    stroke
                );
            }
            break;
        }
        case SceneObjectIcon::Terrain:
        {
            const float baseY = y0 + size * 0.80f;
            dl->AddTriangleFilled(
                ImVec2(x0 + size * 0.06f, baseY),
                ImVec2(x0 + size * 0.40f, y0 + size * 0.22f),
                ImVec2(x0 + size * 0.74f, baseY),
                col
            );
            dl->AddTriangleFilled(
                ImVec2(x0 + size * 0.48f, baseY),
                ImVec2(x0 + size * 0.72f, y0 + size * 0.40f),
                ImVec2(x0 + size * 0.94f, baseY),
                col
            );
            break;
        }
        }
    }

    void DrawPowerIcon(ImDrawList* dl, const ImVec2& center, float size, ImU32 col)
    {
        constexpr float pi = 3.14159265358979323846f;
        const float radius = size * 0.36f;
        const float stroke = std::max(1.2f, size * 0.12f);

        dl->PathArcTo(center, radius, -pi * 0.25f, pi * 1.25f, 12);
        dl->PathStroke(col, 0, stroke);
        dl->AddLine(
            ImVec2(center.x, center.y - radius * 1.15f),
            ImVec2(center.x, center.y - radius * 0.10f),
            col,
            stroke
        );
    }

    void DrawEyeIcon(ImDrawList* dl, const ImVec2& center, float size, ImU32 col, bool visible)
    {
        const float halfW  = size * 0.46f;
        const float curveH = size * 0.34f;
        const float stroke = std::max(1.2f, size * 0.11f);
        const ImVec2 left(center.x - halfW, center.y);
        const ImVec2 right(center.x + halfW, center.y);

        dl->AddBezierCubic(
            left,
            ImVec2(center.x - halfW * 0.45f, center.y - curveH),
            ImVec2(center.x + halfW * 0.45f, center.y - curveH),
            right,
            col, stroke, 8
        );
        dl->AddBezierCubic(
            left,
            ImVec2(center.x - halfW * 0.45f, center.y + curveH),
            ImVec2(center.x + halfW * 0.45f, center.y + curveH),
            right,
            col, stroke, 8
        );
        dl->AddCircleFilled(center, std::max(1.5f, size * 0.15f), col);

        if (!visible)
        {
            const float slash = size * 0.38f;
            dl->AddLine(
                ImVec2(center.x - slash, center.y + slash),
                ImVec2(center.x + slash, center.y - slash),
                col,
                stroke
            );
        }
    }

    void DrawCurrentWindowTabIcon(PanelIcon icon)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImGuiDockNode* node = window ? window->DockNode : nullptr;
        if (!node || !node->TabBar || !node->HostWindow || !node->HostWindow->DrawList)
        {
            return;
        }

        for (const ImGuiTabItem& tab : node->TabBar->Tabs)
        {
            if (tab.Window == window || tab.ID == window->TabId)
            {
                const ImRect& barRect = node->TabBar->BarRect;
                const float iconSize  = ImGui::GetFontSize() * 0.80f;
                const float tabLeftX  = barRect.Min.x + tab.Offset - node->TabBar->ScrollingAnim + ImGui::GetStyle().FramePadding.x;
                const ImVec2 center(tabLeftX + iconSize * 0.5f, (barRect.Min.y + barRect.Max.y) * 0.5f);
                DrawPanelIcon(node->HostWindow->DrawList, icon, center, iconSize, PanelIconColour);
                break;
            }
        }
    }

    bool CompactMenuItem(const char* label, const char* shortcut, bool enabled)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
        {
            return false;
        }

        ImGuiContext& g       = *GImGui;
        ImGuiStyle& style     = g.Style;
        const ImVec2 pos      = window->DC.CursorPos;
        const char* labelEnd  = ImGui::FindRenderedTextEnd(label);
        const ImVec2 labelSz  = ImGui::CalcTextSize(label, labelEnd, false);
        const float shortcutW = (shortcut && shortcut[0]) ? ImGui::CalcTextSize(shortcut, nullptr).x : 0.0f;

        ImGui::PushID(label);
        if (!enabled)
        {
            ImGui::BeginDisabled();
        }

        const float backupRounding = style.SelectableRounding;
        style.SelectableRounding   = style.MenuItemRounding;
        const ImGuiSelectableFlags selectableFlags =
            static_cast<ImGuiSelectableFlags>(ImGuiSelectableFlags_SelectOnRelease) |
            static_cast<ImGuiSelectableFlags>(ImGuiSelectableFlags_SetNavIdOnHover) |
            static_cast<ImGuiSelectableFlags>(ImGuiSelectableFlags_SpanAvailWidth);

        ImGuiMenuColumns* offsets = &window->DC.MenuColumns;
        const float minW          = offsets->DeclColumns(0.0f, labelSz.x, shortcutW, 0.0f);
        const float availW        = ImGui::GetContentRegionAvail().x;
        const float rowW          = std::max(minW, availW);
        const ImVec2 textPos(pos.x, pos.y + window->DC.CurrLineTextBaseOffset);
        const bool pressed        = ImGui::Selectable("", false, selectableFlags, ImVec2(minW, labelSz.y));

        if (g.LastItemData.StatusFlags & ImGuiItemStatusFlags_Visible)
        {
            ImGui::RenderText(ImVec2(textPos.x + offsets->OffsetLabel, textPos.y), label, labelEnd, false);
            if (shortcutW > 0.0f)
            {
                const float shortcutX = pos.x + rowW - shortcutW;
                ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
                ImGui::RenderText(ImVec2(shortcutX, textPos.y), shortcut, nullptr, false);
                ImGui::PopStyleColor();
            }
        }

        style.SelectableRounding = backupRounding;
        if (!enabled)
        {
            ImGui::EndDisabled();
        }
        ImGui::PopID();
        return pressed;
    }

    bool ContainsCaseInsensitive(std::string_view haystack, std::string_view needle)
    {
        if (needle.empty())
        {
            return true;
        }

        const auto it = std::search(
            haystack.begin(), haystack.end(),
            needle.begin(), needle.end(),
            [](char a, char b)
            {
                return std::tolower(static_cast<unsigned char>(a)) ==
                       std::tolower(static_cast<unsigned char>(b));
            }
        );
        return it != haystack.end();
    }
}
