// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "SceneHierarchyPanel.h"
#include "EditorWidgets.h"

#include "Engine/Base.h"
#include "Engine/Body.h"
#include "Engine/Camera.h"
#include "Engine/Character.h"
#include "Engine/Light.h"
#include "Engine/Scene.h"
#include "Engine/Terrain/TerrainObject.h"

#include <imgui.h>

#include <algorithm>
#include <cstring>

namespace Sandbox3D::Editor
{
    namespace
    {
        constexpr const char* SceneHierarchyWindowLabel = "      Scene Hierarchy###Scene Hierarchy";

        bool MatchesHierarchyFilterRecursive(const Engine::Base& object, std::string_view filterView)
        {
            if (filterView.empty() || ContainsCaseInsensitive(object.GetName(), filterView))
            {
                return true;
            }

            for (const auto& child : object.GetChildren())
            {
                if (child && MatchesHierarchyFilterRecursive(*child, filterView))
                {
                    return true;
                }
            }
            return false;
        }
    }

    void SceneHierarchyPanel::ResetTransientState() noexcept
    {
        m_renamingObjectId = 0;
        m_focusRenameInput = false;
    }

    void SceneHierarchyPanel::CreateObject(EditorContext& context, CreateObjectKind kind, const std::shared_ptr<Engine::Base>& parent)
    {
        if (!context.scene)
        {
            return;
        }

        std::shared_ptr<Engine::Base> created;
        switch (kind)
        {
        case CreateObjectKind::EmptyObject:
            created = context.scene->CreateObject<Engine::Base>("Object");
            break;
        case CreateObjectKind::Body:
            created = context.scene->CreateBody<Engine::Body>("Body");
            break;
        case CreateObjectKind::Camera:
            created = context.scene->CreateObject<Engine::Camera>("Camera");
            break;
        case CreateObjectKind::Character:
            created = context.scene->CreateBody<Engine::Character>("Character");
            break;
        case CreateObjectKind::Light:
            created = context.scene->CreateLight(
                Maths::Vec3D(0.0, 10.0, 0.0),
                50.0f,
                Maths::Vec4(1.0f, 1.0f, 1.0f, 1.0f),
                "Light"
            );
            break;
        case CreateObjectKind::TerrainObject:
            created = context.scene->CreateTerrain(Engine::Terrain::TerrainConfig{}, "Terrain");
            break;
        }

        if (created)
        {
            if (parent)
            {
                Engine::Base::SetParent(created, parent);
            }
            context.selectedObjectId = created->GetId();
        }
    }

    void SceneHierarchyPanel::BeginRename(const Engine::Base& object)
    {
        m_renamingObjectId = object.GetId();
        m_focusRenameInput = true;
        const std::string& currentName = object.GetName();
        const size_t copyLen = std::min(currentName.size(), m_renameBuffer.size() - 1);
        std::memcpy(m_renameBuffer.data(), currentName.data(), copyLen);
        m_renameBuffer[copyLen] = '\0';
    }

    void SceneHierarchyPanel::DrawNode(
        EditorContext& context,
        const std::shared_ptr<Engine::Base>& object,
        std::string_view filterView,
        uint32_t& pendingDeleteId)
    {
        if (!object || !MatchesHierarchyFilterRecursive(*object, filterView))
        {
            return;
        }

        const uint32_t objId     = object->GetId();
        const bool isSelected    = (objId == context.selectedObjectId);
        const bool isActive      = object->IsActive();
        const bool isVisible     = object->IsVisible();
        const bool hasChildren   = !object->GetChildren().empty();
        const ImU32 mutedColour  = ImGui::GetColorU32(ImGuiCol_TextDisabled);
        const ImU32 activeColour = isActive ? PanelIconColour : mutedColour;
        const float iconSize     = ImGui::GetFontSize() * 0.80f;

        if (m_renamingObjectId == objId)
        {
            ImGui::PushID(static_cast<int>(objId));
            const float indentOffset = ImGui::GetTreeNodeToLabelSpacing();
            const float rowStartX    = ImGui::GetCursorScreenPos().x + indentOffset;
            const float rowCenterY   = ImGui::GetCursorScreenPos().y + ImGui::GetFrameHeight() * 0.5f;

            DrawSceneObjectIcon(
                ImGui::GetWindowDrawList(),
                ResolveSceneObjectIcon(*object),
                ImVec2(rowStartX + iconSize * 0.5f, rowCenterY),
                iconSize,
                activeColour
            );

            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indentOffset + iconSize + 6.0f);
            ImGui::SetNextItemWidth(std::max(80.0f, ImGui::GetContentRegionAvail().x));
            if (m_focusRenameInput)
            {
                ImGui::SetKeyboardFocusHere();
                m_focusRenameInput = false;
            }

            const bool enterPressed = ImGui::InputText(
                "##RenameObject",
                m_renameBuffer.data(),
                m_renameBuffer.size(),
                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll
            );

            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                m_renamingObjectId = 0;
            }
            else if (enterPressed || ImGui::IsItemDeactivatedAfterEdit())
            {
                if (m_renameBuffer[0] != '\0')
                {
                    object->SetName(m_renameBuffer.data());
                }
                m_renamingObjectId = 0;
            }
            ImGui::PopID();

            if (hasChildren)
            {
                ImGui::TreePush(reinterpret_cast<const void*>(static_cast<uintptr_t>(objId)));
                for (const auto& child : object->GetChildren())
                {
                    DrawNode(context, child, filterView, pendingDeleteId);
                }
                ImGui::TreePop();
            }
            return;
        }

        ImGuiTreeNodeFlags nodeFlags =
            ImGuiTreeNodeFlags_SpanAvailWidth |
            ImGuiTreeNodeFlags_AllowOverlap;
        if (hasChildren)
        {
            nodeFlags |= ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow;
        }
        else
        {
            nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }
        if (isSelected)
        {
            nodeFlags |= ImGuiTreeNodeFlags_Selected;
        }

        const SceneObjectIcon objectIcon = ResolveSceneObjectIcon(*object);
        const std::string& objName       = object->GetName();
        const char* rawName              = objName.empty() ? "<Unnamed>" : objName.c_str();
        const std::string displayLabel   = std::string("     ") + rawName;

        if (!isActive)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, mutedColour);
        }
        if (isSelected)
        {
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_Header));
        }
        const bool nodeOpen = ImGui::TreeNodeEx(
            reinterpret_cast<const void*>(static_cast<uintptr_t>(objId)),
            nodeFlags,
            "%s",
            displayLabel.c_str()
        );
        if (isSelected)
        {
            ImGui::PopStyleColor();
        }
        if (!isActive)
        {
            ImGui::PopStyleColor();
        }

        const bool rowLeftClicked  = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        const bool rowRightClicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
        if (rowRightClicked)
        {
            context.selectedObjectId = objId;
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            BeginRename(*object);
        }

        const float itemMinX    = ImGui::GetItemRectMin().x;
        const float itemMaxX    = ImGui::GetItemRectMax().x;
        const float itemMinY    = ImGui::GetItemRectMin().y;
        const float itemMaxY    = ImGui::GetItemRectMax().y;
        const ImVec2 nextCursor = ImGui::GetCursorScreenPos();
        const float labelStartX = itemMinX + ImGui::GetTreeNodeToLabelSpacing();

        const ImVec2 iconCenter(labelStartX + iconSize * 0.5f, (itemMinY + itemMaxY) * 0.5f);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        DrawSceneObjectIcon(drawList, objectIcon, iconCenter, iconSize, activeColour);

        if (ImGui::BeginPopupContextItem())
        {
            if (CompactMenuItem("Rename", "F2"))
            {
                BeginRename(*object);
            }
            if (ImGui::BeginMenu("Create Child"))
            {
                if (CompactMenuItem("Empty Object"))
                {
                    CreateObject(context, CreateObjectKind::EmptyObject, object);
                }
                if (CompactMenuItem("Light"))
                {
                    CreateObject(context, CreateObjectKind::Light, object);
                }
                if (CompactMenuItem("Camera"))
                {
                    CreateObject(context, CreateObjectKind::Camera, object);
                }
                ImGui::EndMenu();
            }
            if (object->GetParent() != nullptr && CompactMenuItem("Unparent"))
            {
                Engine::Base::SetParent(object, nullptr);
            }
            ImGui::Separator();
            if (CompactMenuItem("Delete", "Del"))
            {
                pendingDeleteId = objId;
            }
            ImGui::EndPopup();
        }

        // Inline Active and Visible toggle buttons on the right
        const float rowHeight    = itemMaxY - itemMinY;
        const float toggleSize   = rowHeight;
        const float toggleGap    = 2.0f;
        const float rightPadding = ImGui::GetStyle().FramePadding.x;
        const float visibleBtnX  = itemMaxX - rightPadding - toggleSize;
        const float activeBtnX   = visibleBtnX - toggleGap - toggleSize;
        const float textMinX     = labelStartX + ImGui::CalcTextSize(displayLabel.c_str()).x + rightPadding;

        bool toggledAny = false;
        if (activeBtnX > textMinX)
        {
            ImGui::PushID(static_cast<int>(objId));

            ImGui::SetCursorScreenPos(ImVec2(activeBtnX, itemMinY));
            ImGui::InvisibleButton("##ActiveToggle", ImVec2(toggleSize, toggleSize));
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                object->SetActive(!isActive);
                toggledAny = true;
            }
            ImGui::SetItemTooltip(isActive ? "Active (click to deactivate)" : "Inactive (click to activate)");
            DrawPowerIcon(
                drawList,
                ImVec2(activeBtnX + toggleSize * 0.5f, (itemMinY + itemMaxY) * 0.5f),
                iconSize * 0.90f,
                isActive ? PanelIconColour : mutedColour
            );

            ImGui::SetCursorScreenPos(ImVec2(visibleBtnX, itemMinY));
            ImGui::InvisibleButton("##VisibleToggle", ImVec2(toggleSize, toggleSize));
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                object->SetVisible(!isVisible);
                toggledAny = true;
            }
            ImGui::SetItemTooltip(isVisible ? "Visible (click to hide)" : "Hidden (click to show)");
            DrawEyeIcon(
                drawList,
                ImVec2(visibleBtnX + toggleSize * 0.5f, (itemMinY + itemMaxY) * 0.5f),
                iconSize * 0.95f,
                isVisible ? PanelIconColour : mutedColour,
                isVisible
            );

            ImGui::PopID();

            ImGui::SetCursorScreenPos(nextCursor);
            ImGui::Dummy(ImVec2(0.0f, 0.0f));
        }

        if (rowLeftClicked && !toggledAny)
        {
            context.selectedObjectId = objId;
        }

        // Right-aligned muted type badge to the left of the toggles
        const std::string_view typeName = object->GetTypeName();
        const std::string typeTag(typeName);
        const float typeTextWidth = ImGui::CalcTextSize(typeTag.c_str()).x;
        const float tagX          = activeBtnX - rightPadding - typeTextWidth;

        if (tagX > textMinX && tagX < activeBtnX)
        {
            const float tagY = (itemMinY + itemMaxY - ImGui::GetFontSize()) * 0.5f;
            drawList->AddText(
                ImVec2(tagX, tagY),
                mutedColour,
                typeTag.c_str()
            );
        }

        if (hasChildren && nodeOpen)
        {
            for (const auto& child : object->GetChildren())
            {
                DrawNode(context, child, filterView, pendingDeleteId);
            }
            ImGui::TreePop();
        }
    }

    void SceneHierarchyPanel::Draw(EditorContext& context, bool& isOpen, ImFont* italicFont)
    {
        if (ImGui::Begin(SceneHierarchyWindowLabel, &isOpen))
        {
            if (!context.scene)
            {
                ImGui::TextDisabled("No scene bound.");
            }
            else
            {
                if (context.selectedObjectId != 0 && !context.scene->FindObject(context.selectedObjectId))
                {
                    context.selectedObjectId = 0;
                }
                if (m_renamingObjectId != 0 && !context.scene->FindObject(m_renamingObjectId))
                {
                    ResetTransientState();
                }

                const ImGuiStyle& style     = ImGui::GetStyle();
                const bool hasFilter        = (m_filter[0] != '\0');
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, 2.0f));
                const float clearButtonSize = ImGui::GetFrameHeight();
                const float filterWidth     = hasFilter
                    ? std::max(40.0f, ImGui::GetContentRegionAvail().x - clearButtonSize - style.ItemSpacing.x)
                    : ImGui::GetContentRegionAvail().x;

                ImGui::SetNextItemWidth(filterWidth);
                ImGui::InputText("##HierarchyFilter", m_filter.data(), m_filter.size());
                if (!hasFilter)
                {
                    const ImVec2 boxMin = ImGui::GetItemRectMin();
                    const ImVec2 boxMax = ImGui::GetItemRectMax();
                    ImFont* hintFont    = italicFont ? italicFont : ImGui::GetFont();
                    const float fontSz  = ImGui::GetFontSize();
                    const ImVec2 hintPos(boxMin.x + style.FramePadding.x, (boxMin.y + boxMax.y - fontSz) * 0.5f);
                    ImGui::GetWindowDrawList()->AddText(
                        hintFont,
                        fontSz,
                        hintPos,
                        ImGui::GetColorU32(ImGuiCol_TextDisabled),
                        "Search"
                    );
                }
                if (hasFilter)
                {
                    ImGui::SameLine();
                    if (ImGui::Button("X##ClearHierarchyFilter", ImVec2(clearButtonSize, clearButtonSize)))
                    {
                        m_filter[0] = '\0';
                    }
                    ImGui::SetItemTooltip("Clear filter");
                }
                ImGui::PopStyleVar();
                ImGui::Separator();

                uint32_t pendingDeleteId = 0;

                if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput && context.selectedObjectId != 0)
                {
                    if (ImGui::IsKeyPressed(ImGuiKey_F2))
                    {
                        if (auto selected = context.scene->FindObject(context.selectedObjectId))
                        {
                            BeginRename(*selected);
                        }
                    }
                    else if (ImGui::IsKeyPressed(ImGuiKey_Delete))
                    {
                        pendingDeleteId = context.selectedObjectId;
                    }
                }

                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetStyleColorVec4(ImGuiCol_Header));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, 4.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, 3.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 16.0f);

                constexpr ImGuiTreeNodeFlags rootFlags =
                    ImGuiTreeNodeFlags_DefaultOpen |
                    ImGuiTreeNodeFlags_SpanAvailWidth;

                const std::string& sceneName = context.scene->GetName();
                const std::string rootLabel  = std::string("     ") + (sceneName.empty() ? "Scene" : sceneName);
                const bool rootOpen          = ImGui::TreeNodeEx("##SceneRoot", rootFlags, "%s", rootLabel.c_str());

                {
                    const float rootMinX    = ImGui::GetItemRectMin().x;
                    const float rootMinY    = ImGui::GetItemRectMin().y;
                    const float rootMaxY    = ImGui::GetItemRectMax().y;
                    const float labelStartX = rootMinX + ImGui::GetTreeNodeToLabelSpacing();
                    const float iconSize    = ImGui::GetFontSize() * 0.80f;
                    const ImVec2 iconCenter(labelStartX + iconSize * 0.5f, (rootMinY + rootMaxY) * 0.5f);
                    DrawSceneObjectIcon(ImGui::GetWindowDrawList(), SceneObjectIcon::Scene, iconCenter, iconSize, PanelIconColour);
                }

                if (ImGui::BeginPopupContextItem("##SceneRootContext"))
                {
                    if (CompactMenuItem("Create Empty Object"))
                    {
                        CreateObject(context, CreateObjectKind::EmptyObject);
                    }
                    if (CompactMenuItem("Create Light"))
                    {
                        CreateObject(context, CreateObjectKind::Light);
                    }
                    if (CompactMenuItem("Create Camera"))
                    {
                        CreateObject(context, CreateObjectKind::Camera);
                    }
                    ImGui::EndPopup();
                }

                if (rootOpen)
                {
                    const std::string_view filterView(m_filter.data());
                    bool anyDisplayed = false;

                    for (const auto& obj : context.scene->GetObjects())
                    {
                        if (!obj || obj->GetParent() != nullptr)
                        {
                            continue;
                        }
                        if (!MatchesHierarchyFilterRecursive(*obj, filterView))
                        {
                            continue;
                        }

                        anyDisplayed = true;
                        DrawNode(context, obj, filterView, pendingDeleteId);
                    }

                    if (!anyDisplayed && !filterView.empty())
                    {
                        ImGui::TextDisabled("No matching objects.");
                    }

                    ImGui::TreePop();
                }

                ImGui::PopStyleVar(3);
                ImGui::PopStyleColor(2);

                if (ImGui::BeginPopupContextWindow("##HierarchyEmptyContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
                {
                    if (CompactMenuItem("Create Empty Object"))
                    {
                        CreateObject(context, CreateObjectKind::EmptyObject);
                    }
                    if (CompactMenuItem("Create Light"))
                    {
                        CreateObject(context, CreateObjectKind::Light);
                    }
                    if (CompactMenuItem("Create Camera"))
                    {
                        CreateObject(context, CreateObjectKind::Camera);
                    }
                    ImGui::EndPopup();
                }

                if (pendingDeleteId != 0)
                {
                    context.scene->RemoveObject(pendingDeleteId);
                    if (context.selectedObjectId == pendingDeleteId || !context.scene->FindObject(context.selectedObjectId))
                    {
                        context.selectedObjectId = 0;
                    }
                    if (m_renamingObjectId == pendingDeleteId || !context.scene->FindObject(m_renamingObjectId))
                    {
                        ResetTransientState();
                    }
                }

                if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
                {
                    context.selectedObjectId = 0;
                    m_renamingObjectId = 0;
                }
            }
        }
        DrawCurrentWindowTabIcon(PanelIcon::SceneHierarchy);
        ImGui::End();
    }
}
