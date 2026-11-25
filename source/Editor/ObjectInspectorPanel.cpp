// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "ObjectInspectorPanel.h"
#include "EditorWidgets.h"
#include "Engine/Base.h"
#include "Engine/Body.h"
#include "Engine/Camera.h"
#include "Engine/Collider.h"
#include "Engine/Light.h"
#include "Engine/LightChannel.h"
#include "Engine/Scene.h"
#include "Engine/Terrain/TerrainCollider.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
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

        bool DrawLightChannelCombo(const char* id, uint32_t& channels, ImFont* italicFont)
        {
            const std::string preview = FormatLightChannels(channels);
            std::array<SearchableOption, Engine::LightChannel::NamedChannels.size() + 2> options{};
            options[0] = { "None", channels == Engine::LightChannel::None };
            options[1] = { "All", (channels & Engine::LightChannel::All) == Engine::LightChannel::All };
            for (size_t i = 0; i < Engine::LightChannel::NamedChannels.size(); ++i)
            {
                const auto& entry = Engine::LightChannel::NamedChannels[i];
                options[i + 2] = { entry.name, (channels & entry.bit) != 0 };
            }

            const int toggled = DrawSearchableChecklistCombo(id, preview.c_str(), options, 4, italicFont);
            if (toggled < 0)
            {
                return false;
            }

            if (toggled == 0)
            {
                channels = Engine::LightChannel::None;
            }
            else if (toggled == 1)
            {
                channels = Engine::LightChannel::All;
            }
            else
            {
                const uint32_t bit = Engine::LightChannel::NamedChannels[static_cast<size_t>(toggled - 2)].bit;
                channels ^= bit;
            }
            return true;
        }

        void InspectorRow(const char* label)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::TableNextColumn();
        }

        const char* FormatUpTo3Decimals(double value) noexcept
        {
            const double scaled = std::abs(value) * 1000.0;
            if (std::isfinite(scaled) && scaled < 9e15)
            {
                const long long rounded = std::llround(scaled);
                if ((rounded % 10LL) != 0LL)
                {
                    return "%.3f";
                }
            }
            return "%.2f";
        }

        bool DragDoublesFormatted(
            const char* id,
            double* values,
            int count,
            float speed,
            const double* minVal = nullptr,
            const double* maxVal = nullptr)
        {
            bool changed = false;
            ImGui::PushID(id);
            const float innerSpacing = ImGui::GetStyle().ItemInnerSpacing.x;
            const float totalWidth   = ImGui::GetContentRegionAvail().x;
            const float itemWidth    = std::max(1.0f, (totalWidth - innerSpacing * static_cast<float>(count - 1)) / static_cast<float>(count));
            for (int i = 0; i < count; ++i)
            {
                ImGui::PushID(i);
                if (i > 0)
                {
                    ImGui::SameLine(0.0f, innerSpacing);
                }
                ImGui::SetNextItemWidth(i == count - 1 ? std::max(1.0f, ImGui::GetContentRegionAvail().x) : itemWidth);
                changed |= ImGui::DragScalar(
                    "##v",
                    ImGuiDataType_Double,
                    &values[i],
                    speed,
                    minVal,
                    maxVal,
                    FormatUpTo3Decimals(values[i])
                );
                ImGui::PopID();
            }
            ImGui::PopID();
            return changed;
        }

        bool DragFloatsFormatted(
            const char* id,
            float* values,
            int count,
            float speed,
            float minVal,
            float maxVal)
        {
            bool changed = false;
            ImGui::PushID(id);
            const float innerSpacing = ImGui::GetStyle().ItemInnerSpacing.x;
            const float totalWidth   = ImGui::GetContentRegionAvail().x;
            const float itemWidth    = std::max(1.0f, (totalWidth - innerSpacing * static_cast<float>(count - 1)) / static_cast<float>(count));
            for (int i = 0; i < count; ++i)
            {
                ImGui::PushID(i);
                if (i > 0)
                {
                    ImGui::SameLine(0.0f, innerSpacing);
                }
                ImGui::SetNextItemWidth(i == count - 1 ? std::max(1.0f, ImGui::GetContentRegionAvail().x) : itemWidth);
                changed |= ImGui::DragFloat(
                    "##v",
                    &values[i],
                    speed,
                    minVal,
                    maxVal,
                    FormatUpTo3Decimals(static_cast<double>(values[i]))
                );
                ImGui::PopID();
            }
            ImGui::PopID();
            return changed;
        }

        void DrawBodySection(Engine::Body& body, ImFont* italicFont)
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
                if (DragDoublesFormatted("##Velocity", velocity, 3, 0.1f))
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
                if (DrawLightChannelCombo("##LightChannels", channels, italicFont))
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

        void DrawColliderSection(Engine::Body& body, ImFont* italicFont)
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
                    const std::array<SearchableOption, 4> colliderOptions =
                    {{
                        { "None",    !body.HasCollider() },
                        { "Box",     body.IsBoxCollider() },
                        { "Sphere",  body.IsSphereCollider() },
                        { "Capsule", body.IsCapsuleCollider() }
                    }};

                    const int picked = DrawSearchableCombo("##ColliderType", preview.c_str(), colliderOptions, 4, italicFont);
                    if (picked >= 0)
                    {
                        const Maths::Vec3D currentOffset = body.HasCollider()
                            ? body.GetCollider()->GetOffset()
                            : Maths::Vec3D{ 0.0, 0.0, 0.0 };

                        if (picked == 0)
                        {
                            body.RemoveCollider();
                        }
                        else if (picked == 1 && !body.IsBoxCollider())
                        {
                            body.SetBoxCollider(Maths::Vec3D{ 0.5, 0.5, 0.5 }, currentOffset);
                        }
                        else if (picked == 2 && !body.IsSphereCollider())
                        {
                            body.SetSphereCollider(0.5, currentOffset);
                        }
                        else if (picked == 3 && !body.IsCapsuleCollider())
                        {
                            body.SetCapsuleCollider(0.35, 0.70, currentOffset);
                        }
                    }
                }

                if (const auto collider = body.GetCollider())
                {
                    InspectorRow("Offset");
                    const Maths::Vec3D currentOffset = collider->GetOffset();
                    double offset[3] = { currentOffset.x, currentOffset.y, currentOffset.z };
                    if (DragDoublesFormatted("##ColliderOffset", offset, 3, 0.05f))
                    {
                        collider->SetOffset(Maths::Vec3D(offset[0], offset[1], offset[2]));
                    }

                    if (auto* box = collider->As<Engine::BoxCollider>())
                    {
                        InspectorRow("Half Extents");
                        const Maths::Vec3D currentExtents = box->GetHalfExtents();
                        double extents[3] = { currentExtents.x, currentExtents.y, currentExtents.z };
                        constexpr double minExtent = 0.001;
                        if (DragDoublesFormatted("##BoxHalfExtents", extents, 3, 0.05f, &minExtent))
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
                        if (ImGui::DragScalar("##SphereRadius", ImGuiDataType_Double, &radius, 0.05f, &minRadius, nullptr, FormatUpTo3Decimals(radius)))
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
                        if (ImGui::DragScalar("##CapsuleRadius", ImGuiDataType_Double, &radius, 0.02f, &minRadius, nullptr, FormatUpTo3Decimals(radius)))
                        {
                            capsule->SetRadius(std::max(minRadius, radius));
                        }

                        InspectorRow("Cylinder Height");
                        double cylHeight = capsule->GetCylinderHeight();
                        constexpr double minHeight = 0.0;
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                        if (ImGui::DragScalar("##CapsuleCylHeight", ImGuiDataType_Double, &cylHeight, 0.05f, &minHeight, nullptr, FormatUpTo3Decimals(cylHeight)))
                        {
                            capsule->SetCylinderHeight(std::max(minHeight, cylHeight));
                        }

                        InspectorRow("Total Height");
                        ImGui::AlignTextToFramePadding();
                        const double totalHeight = capsule->GetTotalHeight();
                        ImGui::TextDisabled(FormatUpTo3Decimals(totalHeight), totalHeight);
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

        void DrawCameraSection(
            Engine::Camera& camera,
            const std::shared_ptr<Engine::Base>& selected,
            EditorContext& context,
            ImFont* italicFont)
        {
            if (!ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
            {
                return;
            }

            if (BeginInspectorTable("##CameraTable"))
            {
                if (context.scene)
                {
                    InspectorRow("Active Camera");
                    if (context.scene->GetCamera() == &camera)
                    {
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextDisabled("Active");
                    }
                    else if (ImGui::Button("Set Active", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                    {
                        if (auto cameraPtr = std::dynamic_pointer_cast<Engine::Camera>(selected))
                        {
                            context.scene->SetCamera(std::move(cameraPtr));
                        }
                    }
                }

                InspectorRow("Projection");
                const bool isPerspective = camera.IsPerspective();
                const std::array<SearchableOption, 2> projectionOptions =
                {{
                    { "Perspective",  isPerspective },
                    { "Orthographic", !isPerspective }
                }};
                const int pickedProj = DrawSearchableCombo(
                    "##CameraProjection",
                    isPerspective ? "Perspective" : "Orthographic",
                    projectionOptions,
                    4,
                    italicFont
                );
                if (pickedProj == 0 && !isPerspective)
                {
                    camera.SetPerspective(camera.GetFovY(), camera.GetAspectRatio(), camera.GetNearZ(), camera.GetFarZ());
                }
                else if (pickedProj == 1 && isPerspective)
                {
                    camera.SetOrthographic(camera.GetOrthoWidth(), camera.GetOrthoHeight(), camera.GetNearZ(), camera.GetFarZ());
                }

                if (camera.IsPerspective())
                {
                    InspectorRow("Field of View");
                    float fovDeg = Maths::ToDegrees(camera.GetFovY());
                    const float aspectRatio = camera.GetAspectRatio();
                    char aspectValueBuf[16]{};
                    std::snprintf(aspectValueBuf, sizeof(aspectValueBuf), FormatUpTo3Decimals(static_cast<double>(aspectRatio)), aspectRatio);
                    char aspectBuf[32]{};
                    std::snprintf(aspectBuf, sizeof(aspectBuf), "Aspect: %s", aspectValueBuf);
                    const float spacing     = ImGui::GetStyle().ItemSpacing.x;
                    const float aspectWidth = ImGui::CalcTextSize(aspectBuf).x;
                    const float fovWidth    = std::max(60.0f, ImGui::GetContentRegionAvail().x - aspectWidth - spacing);

                    ImGui::SetNextItemWidth(fovWidth);
                    if (ImGui::DragFloat("##CameraFov", &fovDeg, 0.5f, 1.0f, 179.0f, "%.1f"))
                    {
                        fovDeg = std::clamp(fovDeg, 1.0f, 179.0f);
                        camera.SetPerspective(Maths::ToRadians(fovDeg), camera.GetAspectRatio(), camera.GetNearZ(), camera.GetFarZ());
                    }
                    ImGui::SameLine();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextDisabled("%s", aspectBuf);
                }
                else
                {
                    InspectorRow("Ortho Size");
                    float orthoSize[2] = { camera.GetOrthoWidth(), camera.GetOrthoHeight() };
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    if (ImGui::DragFloat2("##CameraOrthoSize", orthoSize, 0.1f, 0.01f, 100000.0f, "%.2f"))
                    {
                        const float width  = std::max(0.01f, orthoSize[0]);
                        const float height = std::max(0.01f, orthoSize[1]);
                        camera.SetOrthographic(width, height, camera.GetNearZ(), camera.GetFarZ());
                    }
                }

                auto applyClipPlanes = [&camera](float nearZ, float farZ)
                {
                    if (camera.IsPerspective())
                    {
                        camera.SetPerspective(camera.GetFovY(), camera.GetAspectRatio(), nearZ, farZ);
                    }
                    else
                    {
                        camera.SetOrthographic(camera.GetOrthoWidth(), camera.GetOrthoHeight(), nearZ, farZ);
                    }
                };

                InspectorRow("Near Clip");
                float nearZ = camera.GetNearZ();
                const float maxNear = std::max(0.002f, camera.GetFarZ() - 0.001f);
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if (ImGui::DragFloat("##CameraNear", &nearZ, 0.01f, 0.001f, maxNear, FormatUpTo3Decimals(static_cast<double>(nearZ))))
                {
                    nearZ = std::clamp(nearZ, 0.001f, maxNear);
                    applyClipPlanes(nearZ, camera.GetFarZ());
                }

                InspectorRow("Far Clip");
                float farZ = camera.GetFarZ();
                const float minFar = camera.GetNearZ() + 0.001f;
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if (ImGui::DragFloat("##CameraFar", &farZ, 10.0f, minFar, 1000000.0f, "%.1f"))
                {
                    farZ = std::clamp(farZ, minFar, 1000000.0f);
                    applyClipPlanes(camera.GetNearZ(), farZ);
                }

                ImGui::EndTable();
            }
        }

        void DrawLightSection(Engine::Light& light, ImFont* italicFont)
        {
            if (!ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
            {
                return;
            }

            if (BeginInspectorTable("##LightTable"))
            {
                InspectorRow("Type");
                const Engine::LightType currentType = light.GetLightType();
                const std::string_view typeName = Engine::GetLightTypeName(currentType);
                const std::string preview(typeName);
                const std::array<SearchableOption, 3> typeOptions =
                {{
                    { "Directional", currentType == Engine::LightType::Directional },
                    { "Point",       currentType == Engine::LightType::Point },
                    { "Spot",        currentType == Engine::LightType::Spot }
                }};
                const int pickedType = DrawSearchableCombo("##LightType", preview.c_str(), typeOptions, 4, italicFont);
                if (pickedType == 0)
                {
                    light.SetLightType(Engine::LightType::Directional);
                }
                else if (pickedType == 1)
                {
                    light.SetLightType(Engine::LightType::Point);
                }
                else if (pickedType == 2)
                {
                    light.SetLightType(Engine::LightType::Spot);
                }

                InspectorRow("Light Channels");
                uint32_t channels = light.GetChannels();
                if (DrawLightChannelCombo("##LightChannels", channels, italicFont))
                {
                    light.SetChannels(channels);
                }

                InspectorRow("Colour");
                const Maths::Vec4 currentColour = light.GetColor();
                float colour[3] = { currentColour.x, currentColour.y, currentColour.z };
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if (ImGui::ColorEdit3("##LightColour", colour))
                {
                    light.SetColor(Maths::Vec4(colour[0], colour[1], colour[2], currentColour.w));
                }

                InspectorRow("Temperature");
                float kelvin = light.GetColourTemperature();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if (ImGui::DragFloat("##LightTemperature", &kelvin, 25.0f, 1000.0f, 40000.0f, "%.0f K"))
                {
                    light.SetColourTemperature(std::clamp(kelvin, 1000.0f, 40000.0f));
                }

                InspectorRow("Intensity");
                float intensity = light.GetIntensity();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if (ImGui::DragFloat("##LightIntensity", &intensity, 0.05f, 0.0f, 1000.0f, "%.2f"))
                {
                    light.SetIntensity(std::max(0.0f, intensity));
                }

                const Engine::LightType activeType = light.GetLightType();

                if (activeType == Engine::LightType::Directional || activeType == Engine::LightType::Spot)
                {
                    InspectorRow("Direction");
                    const Maths::Vec3 currentDir = light.GetDirection3();
                    float dir[3] = { currentDir.x, currentDir.y, currentDir.z };
                    if (DragFloatsFormatted("##LightDirection", dir, 3, 0.02f, -1.0f, 1.0f))
                    {
                        const Maths::Vec3 newDir(dir[0], dir[1], dir[2]);
                        if (newDir.LengthSquared() > 1e-6f)
                        {
                            light.SetDirection(newDir);
                        }
                    }
                }

                if (activeType == Engine::LightType::Point || activeType == Engine::LightType::Spot)
                {
                    InspectorRow("Range");
                    float range = light.GetRange();
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    if (ImGui::DragFloat("##LightRange", &range, 0.5f, 0.01f, 100000.0f, "%.2f"))
                    {
                        light.SetRange(std::max(0.01f, range));
                    }

                    InspectorRow("Attenuation");
                    const Maths::Vec4 currentAtten = light.GetAttenuation();
                    if (activeType == Engine::LightType::Point)
                    {
                        float atten[3] = { currentAtten.x, currentAtten.y, currentAtten.z };
                        if (DragFloatsFormatted("##LightAttenuation", atten, 3, 0.005f, 0.0f, 10.0f))
                        {
                            light.SetAttenuation(
                                std::max(0.0f, atten[0]),
                                std::max(0.0f, atten[1]),
                                std::max(0.0f, atten[2])
                            );
                        }
                    }
                    else
                    {
                        float atten[2] = { currentAtten.x, currentAtten.y };
                        if (DragFloatsFormatted("##LightAttenuation", atten, 2, 0.005f, 0.0f, 10.0f))
                        {
                            light.SetAttenuation(
                                std::max(0.0f, atten[0]),
                                std::max(0.0f, atten[1]),
                                currentAtten.z
                            );
                        }
                    }
                }

                if (activeType == Engine::LightType::Spot)
                {
                    float innerDeg = Maths::ToDegrees(light.GetInnerSpotAngle());
                    float outerDeg = Maths::ToDegrees(light.GetOuterSpotAngle());

                    InspectorRow("Inner Angle");
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    if (ImGui::DragFloat("##LightInnerAngle", &innerDeg, 0.5f, 0.0f, 89.5f, "%.1f deg"))
                    {
                        innerDeg = std::clamp(innerDeg, 0.0f, 89.5f);
                        outerDeg = std::max(outerDeg, innerDeg);
                        light.SetSpotAngles(Maths::ToRadians(innerDeg), Maths::ToRadians(outerDeg));
                    }

                    InspectorRow("Outer Angle");
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    if (ImGui::DragFloat("##LightOuterAngle", &outerDeg, 0.5f, 0.0f, 89.5f, "%.1f deg"))
                    {
                        outerDeg = std::clamp(outerDeg, 0.0f, 89.5f);
                        innerDeg = std::min(innerDeg, outerDeg);
                        light.SetSpotAngles(Maths::ToRadians(innerDeg), Maths::ToRadians(outerDeg));
                    }
                }

                ImGui::EndTable();
            }
        }
    }

    void ObjectInspectorPanel::Draw(EditorContext& context, bool& isOpen, ImFont* italicFont)
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

                // Transform section (64-bit world matrix: position, Euler rotation, and scale)
                if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    if (BeginInspectorTable("##TransformTable"))
                    {
                        const Maths::Mat4x4D& worldMatrix = selected->GetWorldMatrix();
                        const Maths::Vec3D currentPos = selected->GetPosition();
                        const Maths::Vec3D currentEulerRad = worldMatrix.GetEulerAngles();
                        const Maths::Vec3D currentScale(
                            Maths::Vec3D(worldMatrix.m[0][0], worldMatrix.m[0][1], worldMatrix.m[0][2]).Length(),
                            Maths::Vec3D(worldMatrix.m[1][0], worldMatrix.m[1][1], worldMatrix.m[1][2]).Length(),
                            Maths::Vec3D(worldMatrix.m[2][0], worldMatrix.m[2][1], worldMatrix.m[2][2]).Length()
                        );

                        InspectorRow("Position");
                        double position[3] = { currentPos.x, currentPos.y, currentPos.z };
                        if (DragDoublesFormatted("##Position", position, 3, 0.1f))
                        {
                            selected->SetPosition(Maths::Vec3D(position[0], position[1], position[2]));
                        }

                        InspectorRow("Rotation");
                        double rotationDeg[3] = {
                            Maths::ToDegrees(currentEulerRad.x),
                            Maths::ToDegrees(currentEulerRad.y),
                            Maths::ToDegrees(currentEulerRad.z)
                        };
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                        if (ImGui::DragScalarN("##Rotation", ImGuiDataType_Double, rotationDeg, 3, 0.5f, nullptr, nullptr, "%.2f"))
                        {
                            const Maths::Vec3D safeScale(
                                std::max(0.001, currentScale.x),
                                std::max(0.001, currentScale.y),
                                std::max(0.001, currentScale.z)
                            );
                            Maths::Mat4x4D updated = Maths::Mat4x4D::Scale(safeScale)
                                * Maths::Mat4x4D::RotationYawPitchRoll(
                                    Maths::ToRadians(rotationDeg[1]),
                                    Maths::ToRadians(rotationDeg[0]),
                                    Maths::ToRadians(rotationDeg[2])
                                );
                            updated.SetTranslation(currentPos);
                            selected->SetWorldMatrix(updated);
                        }

                        InspectorRow("Scale");
                        double scale[3] = { currentScale.x, currentScale.y, currentScale.z };
                        constexpr double minScale = 0.001;
                        if (DragDoublesFormatted("##Scale", scale, 3, 0.02f, &minScale))
                        {
                            const Maths::Vec3D newScale(
                                std::max(minScale, scale[0]),
                                std::max(minScale, scale[1]),
                                std::max(minScale, scale[2])
                            );
                            Maths::Mat4x4D updated = Maths::Mat4x4D::Scale(newScale)
                                * Maths::Mat4x4D::RotationYawPitchRoll(
                                    currentEulerRad.y,
                                    currentEulerRad.x,
                                    currentEulerRad.z
                                );
                            updated.SetTranslation(currentPos);
                            selected->SetWorldMatrix(updated);
                        }

                        ImGui::EndTable();
                    }
                }

                if (auto* body = dynamic_cast<Engine::Body*>(selected.get()))
                {
                    DrawBodySection(*body, italicFont);
                    DrawColliderSection(*body, italicFont);
                }

                if (auto* camera = dynamic_cast<Engine::Camera*>(selected.get()))
                {
                    DrawCameraSection(*camera, selected, context, italicFont);
                }

                if (auto* light = dynamic_cast<Engine::Light*>(selected.get()))
                {
                    DrawLightSection(*light, italicFont);
                }

                ImGui::PopID();
                ImGui::PopStyleVar(2);
            }
        }
        DrawCurrentWindowTabIcon(PanelIcon::ObjectInspector);
        ImGui::End();
    }
}
