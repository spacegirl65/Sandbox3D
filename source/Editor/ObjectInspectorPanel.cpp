// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "ObjectInspectorPanel.h"
#include "EditorWidgets.h"
#include "Engine/Base.h"
#include "Engine/Body.h"
#include "Engine/Collider.h"
#include "Engine/LightChannel.h"
#include "Engine/Scene.h"
#include "Engine/Terrain/TerrainCollider.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <string>

namespace Sandbox3D::Editor
{
    namespace
    {
        constexpr const char* ObjectInspectorWindowLabel = "      Object Inspector###Object Inspector";

        bool BeginInspectorTable(const char* id)
        {
            if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_None))
            {
                return false;
            }
            const float labelColumnWidth = std::max(140.0f, ImGui::GetFontSize() * 8.0f);
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, labelColumnWidth);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            return true;
        }

        std::string FormatLightChannels(uint32_t mask)
        {
            if (mask == Engine::LightChannel::None)
            {
                return "None";
            }
            if ((mask & Engine::LightChannel::All) == Engine::LightChannel::All)
            {
                return "All";
            }

            std::string result;
            for (const Engine::LightChannel::Descriptor& entry : Engine::LightChannel::NamedChannels)
            {
                if ((mask & entry.bit) != 0)
                {
                    if (!result.empty())
                    {
                        result += ", ";
                    }
                    result += entry.name;
                }
            }

            return result.empty() ? "Custom" : result;
        }

        bool DrawLightChannelCombo(const char* id, uint32_t& channels)
        {
            bool changed = false;
            const std::string preview = FormatLightChannels(channels);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::BeginCombo(id, preview.c_str()))
            {
                if (ImGui::Selectable("None", channels == Engine::LightChannel::None))
                {
                    channels = Engine::LightChannel::None;
                    changed = true;
                }
                if (ImGui::Selectable("All", (channels & Engine::LightChannel::All) == Engine::LightChannel::All))
                {
                    channels = Engine::LightChannel::All;
                    changed = true;
                }
                ImGui::Separator();

                for (const Engine::LightChannel::Descriptor& entry : Engine::LightChannel::NamedChannels)
                {
                    bool enabled = (channels & entry.bit) != 0;
                    if (ImGui::Checkbox(entry.name, &enabled))
                    {
                        if (enabled)
                        {
                            channels |= entry.bit;
                        }
                        else
                        {
                            channels &= ~entry.bit;
                        }
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
            return changed;
        }

        void InspectorRow(const char* label)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::TableNextColumn();
        }

        void DrawBodySection(Engine::Body& body)
        {
            if (!ImGui::CollapsingHeader("Body", ImGuiTreeNodeFlags_DefaultOpen))
            {
                return;
            }

            if (BeginInspectorTable("##BodyTable"))
            {
                InspectorRow("Velocity");
                const Maths::Vec3D currentVel = body.GetVelocity();
                double velocity[3] = { currentVel.x, currentVel.y, currentVel.z };
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if (ImGui::DragScalarN("##Velocity", ImGuiDataType_Double, velocity, 3, 0.1f, nullptr, nullptr, "%.3f"))
                {
                    body.SetVelocity(Maths::Vec3D(velocity[0], velocity[1], velocity[2]));
                }

                InspectorRow("Grounded");
                bool grounded = body.IsGrounded();
                if (ImGui::Checkbox("##Grounded", &grounded))
                {
                    body.SetGrounded(grounded);
                }

                InspectorRow("Light Channels");
                uint32_t channels = body.GetLightChannels();
                if (DrawLightChannelCombo("##LightChannels", channels))
                {
                    body.SetLightChannels(channels);
                }

                InspectorRow("Mesh");
                ImGui::AlignTextToFramePadding();
                if (const auto mesh = body.GetMesh())
                {
                    const char* meshName = mesh->GetName().empty() ? "<Unnamed Mesh>" : mesh->GetName().c_str();
                    ImGui::TextDisabled("%s", meshName);
                }
                else
                {
                    ImGui::TextDisabled("None");
                }

                InspectorRow("Material");
                ImGui::AlignTextToFramePadding();
                if (const auto material = body.GetMaterial())
                {
                    const char* matName = material->GetName().empty() ? "<Unnamed Material>" : material->GetName().c_str();
                    ImGui::TextDisabled("%s", matName);
                }
                else
                {
                    ImGui::TextDisabled("None");
                }

                ImGui::EndTable();
            }
        }

        void DrawColliderSection(Engine::Body& body)
        {
            if (!ImGui::CollapsingHeader("Collider", ImGuiTreeNodeFlags_DefaultOpen))
            {
                return;
            }

            if (BeginInspectorTable("##ColliderTable"))
            {
                InspectorRow("Type");
                const std::string_view colliderType = body.GetColliderTypeName();
                if (body.IsTerrainCollider())
                {
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(colliderType.data(), colliderType.data() + colliderType.size());
                }
                else
                {
                    const std::string preview(colliderType);
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    if (ImGui::BeginCombo("##ColliderType", preview.c_str()))
                    {
                        const Maths::Vec3D currentOffset = body.HasCollider()
                            ? body.GetCollider()->GetOffset()
                            : Maths::Vec3D{ 0.0, 0.0, 0.0 };

                        if (ImGui::Selectable("None", !body.HasCollider()))
                        {
                            body.RemoveCollider();
                        }
                        if (ImGui::Selectable("Box", body.IsBoxCollider()))
                        {
                            if (!body.IsBoxCollider())
                            {
                                body.SetBoxCollider(Maths::Vec3D{ 0.5, 0.5, 0.5 }, currentOffset);
                            }
                        }
                        if (ImGui::Selectable("Sphere", body.IsSphereCollider()))
                        {
                            if (!body.IsSphereCollider())
                            {
                                body.SetSphereCollider(0.5, currentOffset);
                            }
                        }
                        if (ImGui::Selectable("Capsule", body.IsCapsuleCollider()))
                        {
                            if (!body.IsCapsuleCollider())
                            {
                                body.SetCapsuleCollider(0.35, 0.70, currentOffset);
                            }
                        }
                        ImGui::EndCombo();
                    }
                }

                if (const auto collider = body.GetCollider())
                {
                    InspectorRow("Offset");
                    const Maths::Vec3D currentOffset = collider->GetOffset();
                    double offset[3] = { currentOffset.x, currentOffset.y, currentOffset.z };
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    if (ImGui::DragScalarN("##ColliderOffset", ImGuiDataType_Double, offset, 3, 0.05f, nullptr, nullptr, "%.3f"))
                    {
                        collider->SetOffset(Maths::Vec3D(offset[0], offset[1], offset[2]));
                    }

                    if (auto* box = collider->As<Engine::BoxCollider>())
                    {
                        InspectorRow("Half Extents");
                        const Maths::Vec3D currentExtents = box->GetHalfExtents();
                        double extents[3] = { currentExtents.x, currentExtents.y, currentExtents.z };
                        constexpr double minExtent = 0.001;
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                        if (ImGui::DragScalarN("##BoxHalfExtents", ImGuiDataType_Double, extents, 3, 0.05f, &minExtent, nullptr, "%.3f"))
                        {
                            box->SetHalfExtents(Maths::Vec3D(
                                std::max(minExtent, extents[0]),
                                std::max(minExtent, extents[1]),
                                std::max(minExtent, extents[2])
                            ));
                        }
                    }
                    else if (auto* sphere = collider->As<Engine::SphereCollider>())
                    {
                        InspectorRow("Radius");
                        double radius = sphere->GetRadius();
                        constexpr double minRadius = 0.001;
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                        if (ImGui::DragScalar("##SphereRadius", ImGuiDataType_Double, &radius, 0.05f, &minRadius, nullptr, "%.3f"))
                        {
                            sphere->SetRadius(std::max(minRadius, radius));
                        }
                    }
                    else if (auto* capsule = collider->As<Engine::CapsuleCollider>())
                    {
                        InspectorRow("Radius");
                        double radius = capsule->GetRadius();
                        constexpr double minRadius = 0.001;
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                        if (ImGui::DragScalar("##CapsuleRadius", ImGuiDataType_Double, &radius, 0.02f, &minRadius, nullptr, "%.3f"))
                        {
                            capsule->SetRadius(std::max(minRadius, radius));
                        }

                        InspectorRow("Cylinder Height");
                        double cylHeight = capsule->GetCylinderHeight();
                        constexpr double minHeight = 0.0;
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                        if (ImGui::DragScalar("##CapsuleCylHeight", ImGuiDataType_Double, &cylHeight, 0.05f, &minHeight, nullptr, "%.3f"))
                        {
                            capsule->SetCylinderHeight(std::max(minHeight, cylHeight));
                        }

                        InspectorRow("Total Height");
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextDisabled("%.3f", capsule->GetTotalHeight());
                    }
                    else if (const auto* terrain = collider->As<Engine::Terrain::TerrainCollider>())
                    {
                        InspectorRow("Resolution");
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextDisabled("%u x %u", terrain->GetResolutionX(), terrain->GetResolutionZ());

                        InspectorRow("Footprint");
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextDisabled("%.1f x %.1f", terrain->GetWidth(), terrain->GetDepth());
                    }
                }

                ImGui::EndTable();
            }

            if (const auto mesh = body.GetMesh(); mesh && (body.IsBoxCollider() || body.IsSphereCollider()))
            {
                if (ImGui::Button("Fit To Mesh", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                {
                    if (body.IsBoxCollider())
                    {
                        body.SetCollider(Engine::BoxCollider::CreateFromMesh(*mesh));
                    }
                    else if (body.IsSphereCollider())
                    {
                        body.SetCollider(Engine::SphereCollider::CreateFromMesh(*mesh));
                    }
                }
            }
        }
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
                const ImGuiStyle& style = ImGui::GetStyle();
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, 2.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, 5.0f));
                ImGui::PushID(static_cast<int>(selected->GetId()));

                // Header: class icon + full-width editable name
                const float iconSize   = ImGui::GetFontSize() * 0.85f;
                const float rowHeight  = ImGui::GetFrameHeight();
                const ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                const ImVec2 iconCenter(cursorPos.x + iconSize * 0.5f, cursorPos.y + rowHeight * 0.5f);
                DrawSceneObjectIcon(
                    ImGui::GetWindowDrawList(),
                    ResolveSceneObjectIcon(*selected),
                    iconCenter,
                    iconSize,
                    PanelIconColour
                );
                ImGui::Dummy(ImVec2(iconSize, rowHeight));
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
                    if (BeginInspectorTable("##TransformTable"))
                    {
                        InspectorRow("Position");
                        const Maths::Vec3D currentPos = selected->GetPosition();
                        double position[3] = { currentPos.x, currentPos.y, currentPos.z };
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                        if (ImGui::DragScalarN("##Position", ImGuiDataType_Double, position, 3, 0.1f, nullptr, nullptr, "%.3f"))
                        {
                            selected->SetPosition(Maths::Vec3D(position[0], position[1], position[2]));
                        }

                        ImGui::EndTable();
                    }
                }

                if (auto* body = dynamic_cast<Engine::Body*>(selected.get()))
                {
                    DrawBodySection(*body);
                    DrawColliderSection(*body);
                }

                ImGui::PopID();
                ImGui::PopStyleVar(2);
            }
        }
        DrawCurrentWindowTabIcon(PanelIcon::ObjectInspector);
        ImGui::End();
    }
}
