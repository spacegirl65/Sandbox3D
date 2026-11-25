// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "ViewportPanel.h"
#include "Core/Logger.h"
#include "Engine/Base.h"
#include "Engine/Body.h"
#include "Engine/Collider.h"
#include "Engine/Scene.h"
#include "Maths/Quat.h"
#include "Maths/Ray.h"
#include "Maths/Vec4.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <limits>

namespace Sandbox3D::Editor
{
    namespace
    {
        constexpr const char* ViewportTitle = "Viewport";

        Maths::RayD BuildPickRay(
            const Engine::Camera& camera,
            float normalizedX,
            float normalizedY) noexcept
        {
            const float ndcX = normalizedX * 2.0f - 1.0f;
            const float ndcY = 1.0f - normalizedY * 2.0f;

            const Maths::Mat4x4& proj = camera.GetProjectionMatrix();
            const double projX = (std::abs(proj.m[0][0]) > 1e-6f) ? static_cast<double>(proj.m[0][0]) : 1.0;
            const double projY = (std::abs(proj.m[1][1]) > 1e-6f) ? static_cast<double>(proj.m[1][1]) : 1.0;

            const Maths::Mat4x4D invView = camera.GetViewMatrix().Inverted();

            if (camera.IsPerspective())
            {
                const Maths::Vec3D localDir(
                    static_cast<double>(ndcX) / projX,
                    static_cast<double>(ndcY) / projY,
                    1.0
                );
                const Maths::Vec3D worldDir = invView.TransformDirection(localDir).Normalised();
                return Maths::RayD(camera.GetPosition(), worldDir);
            }

            const Maths::Vec3D localOrigin(
                static_cast<double>(ndcX) / projX,
                static_cast<double>(ndcY) / projY,
                0.0
            );
            const Maths::Vec3D worldOrigin = invView.TransformPoint(localOrigin);
            const Maths::Vec3D worldDir    = invView.TransformDirection(Maths::Vec3D::Forward()).Normalised();
            return Maths::RayD(worldOrigin, worldDir);
        }

        uint32_t PickSceneObject(const Engine::Scene& scene, const Maths::RayD& ray) noexcept
        {
            uint32_t bestId   = 0;
            double bestDist   = std::numeric_limits<double>::max();

            for (const auto& obj : scene.GetObjects())
            {
                if (!obj || !obj->IsActive() || !obj->IsVisible())
                {
                    continue;
                }

                const auto* body = dynamic_cast<const Engine::Body*>(obj.get());
                if (!body)
                {
                    continue;
                }

                double hitDist = 0.0;
                bool hit = false;

                if (const auto mesh = body->GetMesh())
                {
                    const Maths::Mat4x4D& world = body->GetWorldMatrix();
                    const Maths::Mat4x4D invWorld = world.Inverted();
                    const Maths::Vec3D localOrigin = invWorld.TransformPoint(ray.origin);
                    const Maths::Vec3D localDir    = invWorld.TransformDirection(ray.direction);
                    if (localDir.LengthSquared() > 1e-12)
                    {
                        const Maths::RayD localRay(localOrigin, localDir);
                        double localDist = 0.0;
                        const Maths::BoundingBoxD localBounds(mesh->GetBoundingBox());
                        if (localRay.Intersects(localBounds, localDist) && localDist >= 0.0)
                        {
                            const Maths::Vec3D worldHit = world.TransformPoint(localRay.GetPoint(localDist));
                            hitDist = (worldHit - ray.origin).Length();
                            hit = true;
                        }
                    }
                }
                else if (body->HasCollider())
                {
                    hit = body->Intersects(ray, &hitDist) && hitDist >= 0.0;
                }

                if (hit && hitDist < bestDist)
                {
                    bestDist = hitDist;
                    bestId   = obj->GetId();
                }
            }

            return bestId;
        }

        void AccumulateClippedSegmentScreenBounds(
            const Maths::Mat4x4& mvp,
            const Maths::Vec3& localA,
            const Maths::Vec3& localB,
            const ImVec2& viewportMin,
            const ImVec2& viewportSize,
            ImVec2& outMin,
            ImVec2& outMax,
            bool& hasVisiblePoint) noexcept
        {
            Maths::Vec4 clipA = mvp.TransformVector(Maths::Vec4(localA.x, localA.y, localA.z, 1.0f));
            Maths::Vec4 clipB = mvp.TransformVector(Maths::Vec4(localB.x, localB.y, localB.z, 1.0f));

            constexpr float nearClipW = 0.05f;
            if (clipA.w < nearClipW && clipB.w < nearClipW)
            {
                return;
            }

            if (clipA.w < nearClipW)
            {
                const float t = (nearClipW - clipA.w) / (clipB.w - clipA.w);
                clipA = clipA + (clipB - clipA) * t;
            }
            else if (clipB.w < nearClipW)
            {
                const float t = (nearClipW - clipB.w) / (clipA.w - clipB.w);
                clipB = clipB + (clipA - clipB) * t;
            }

            const float invWA = 1.0f / clipA.w;
            const float invWB = 1.0f / clipB.w;
            const ImVec2 screenA(
                viewportMin.x + (clipA.x * invWA * 0.5f + 0.5f) * viewportSize.x,
                viewportMin.y + (0.5f - clipA.y * invWA * 0.5f) * viewportSize.y
            );
            const ImVec2 screenB(
                viewportMin.x + (clipB.x * invWB * 0.5f + 0.5f) * viewportSize.x,
                viewportMin.y + (0.5f - clipB.y * invWB * 0.5f) * viewportSize.y
            );

            if (!hasVisiblePoint)
            {
                outMin = ImVec2(std::min(screenA.x, screenB.x), std::min(screenA.y, screenB.y));
                outMax = ImVec2(std::max(screenA.x, screenB.x), std::max(screenA.y, screenB.y));
                hasVisiblePoint = true;
            }
            else
            {
                outMin.x = std::min({ outMin.x, screenA.x, screenB.x });
                outMin.y = std::min({ outMin.y, screenA.y, screenB.y });
                outMax.x = std::max({ outMax.x, screenA.x, screenB.x });
                outMax.y = std::max({ outMax.y, screenA.y, screenB.y });
            }
        }

        void DrawScreenBoxHighlight(
            ImDrawList* drawList,
            const Maths::Mat4x4& mvp,
            const Maths::BoundingBox& localBox,
            const ImVec2& viewportMin,
            const ImVec2& viewportSize,
            ImU32 borderColour,
            ImU32 fillColour,
            float thickness) noexcept
        {
            std::array<Maths::Vec3, 8> corners{};
            localBox.GetCorners(corners);

            static constexpr std::array<std::pair<size_t, size_t>, 12> edges =
            {{
                { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
                { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
                { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
            }};

            ImVec2 screenMin{};
            ImVec2 screenMax{};
            bool hasVisiblePoint = false;
            for (const auto& [a, b] : edges)
            {
                AccumulateClippedSegmentScreenBounds(
                    mvp,
                    corners[a],
                    corners[b],
                    viewportMin,
                    viewportSize,
                    screenMin,
                    screenMax,
                    hasVisiblePoint
                );
            }

            if (!hasVisiblePoint)
            {
                return;
            }

            constexpr float padding = 4.0f;
            screenMin.x -= padding;
            screenMin.y -= padding;
            screenMax.x += padding;
            screenMax.y += padding;

            drawList->AddRectFilled(screenMin, screenMax, fillColour);
            drawList->AddRect(screenMin, screenMax, borderColour, 0.0f, 0, thickness);
        }

        void DrawSelectionHighlight(
            ImDrawList* drawList,
            const Engine::Camera& camera,
            const Engine::Base& selected,
            const ImVec2& viewportMin,
            const ImVec2& viewportSize) noexcept
        {
            if (!selected.IsActive() || !selected.IsVisible())
            {
                return;
            }

            constexpr ImU32 borderColour  = IM_COL32(27, 79, 161, 235);
            constexpr ImU32 fillColour    = IM_COL32(27, 79, 161, 24);
            constexpr float lineThickness = 1.5f;

            const ImVec2 viewportMax(viewportMin.x + viewportSize.x, viewportMin.y + viewportSize.y);
            drawList->PushClipRect(viewportMin, viewportMax, true);

            const auto* body = dynamic_cast<const Engine::Body*>(&selected);
            if (body && body->GetMesh())
            {
                // Mesh silhouettes are rendered via the GPU selection outline pass
                drawList->PopClipRect();
                return;
            }
            if (body && body->HasCollider())
            {
                const Maths::BoundingBoxD worldBox = body->GetCollider()->GetWorldBoundingBox(body->GetWorldMatrix());
                const Maths::Mat4x4D centerWorld = Maths::Mat4x4D::Translation(worldBox.GetCenter());
                const Maths::Mat4x4 mvp = camera.CalculateCameraRelativeMVP(centerWorld);
                const Maths::Vec3D ext = worldBox.GetExtents();
                const Maths::BoundingBox localBox(
                    Maths::Vec3(static_cast<float>(-ext.x), static_cast<float>(-ext.y), static_cast<float>(-ext.z)),
                    Maths::Vec3(static_cast<float>(ext.x),  static_cast<float>(ext.y),  static_cast<float>(ext.z))
                );
                DrawScreenBoxHighlight(drawList, mvp, localBox, viewportMin, viewportSize, borderColour, fillColour, lineThickness);
            }
            else
            {
                const Maths::Mat4x4 mvp = camera.CalculateCameraRelativeMVP(Maths::Mat4x4D::Translation(selected.GetPosition()));
                const Maths::Vec4 clip = mvp.TransformVector(Maths::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
                if (clip.w >= 0.05f)
                {
                    const float invW = 1.0f / clip.w;
                    const ImVec2 center(
                        viewportMin.x + (clip.x * invW * 0.5f + 0.5f) * viewportSize.x,
                        viewportMin.y + (0.5f - clip.y * invW * 0.5f) * viewportSize.y
                    );
                    constexpr float halfExtent = 8.0f;
                    const ImVec2 rectMin(center.x - halfExtent, center.y - halfExtent);
                    const ImVec2 rectMax(center.x + halfExtent, center.y + halfExtent);
                    drawList->AddRectFilled(rectMin, rectMax, fillColour);
                    drawList->AddRect(rectMin, rectMax, borderColour, 0.0f, 0, lineThickness);
                }
            }

            drawList->PopClipRect();
        }
    }

    void ViewportPanel::Initialise(
        IDXGIFactory6* factory,
        ID3D12Device* device,
        ID3D12CommandQueue* commandQueue,
        const std::wstring& gpuDescription,
        const Renderer::Renderer* sourceRenderer,
        const SrvAllocFn& allocateSrv)
    {
        m_renderer.InitialiseOffscreen(
            factory,
            device,
            commandQueue,
            m_width,
            m_height,
            gpuDescription
        );
        if (sourceRenderer)
        {
            m_renderer.CopyEnvironmentSettingsFrom(*sourceRenderer);
        }

        const float initialAspect = static_cast<float>(m_width) / static_cast<float>(m_height);
        m_camera.SetPerspective(Maths::ToRadians(60.0f), initialAspect, 0.1f, 30000.0f);
        UpdateCamera(0.0f, false, nullptr);
        m_renderer.SetCamera(&m_camera);
        UpdateSrv(device, allocateSrv);
    }

    void ViewportPanel::Shutdown(ID3D12CommandQueue* commandQueue, const SrvFreeFn& freeSrv) noexcept
    {
        m_renderer.Shutdown(commandQueue);

        if (m_hasSrv)
        {
            for (UINT i = 0; i < Renderer::SwapChain::BufferCount; ++i)
            {
                if (freeSrv)
                {
                    freeSrv(m_srvCpuHandles[i], m_srvGpuHandles[i]);
                }
                m_srvCpuHandles[i] = {};
                m_srvGpuHandles[i] = {};
            }
            m_hasSrv = false;
        }
    }

    void ViewportPanel::Render(
        ID3D12Device* device,
        ID3D12CommandQueue* commandQueue,
        std::span<const Renderer::RenderItem> renderItems,
        std::span<const Renderer::GpuLight> lights,
        float deltaTime,
        float updatesPerSecond,
        float targetUps,
        bool windowFocused,
        ImGuiContext* imguiContext,
        const SrvAllocFn& allocateSrv)
    {
        if (m_desiredWidth > 0 && m_desiredHeight > 0 &&
            (m_desiredWidth != m_width || m_desiredHeight != m_height))
        {
            m_width  = m_desiredWidth;
            m_height = m_desiredHeight;
            m_renderer.OnResize(device, commandQueue, m_width, m_height);
            UpdateSrv(device, allocateSrv);
        }

        UpdateCamera(deltaTime, windowFocused, imguiContext);

        m_renderer.Render(
            commandQueue,
            renderItems,
            lights,
            /* vSync = */ false,
            renderItems.size(),
            updatesPerSecond,
            targetUps
        );
    }

    void ViewportPanel::UpdateSrv(ID3D12Device* device, const SrvAllocFn& allocateSrv)
    {
        if (!device)
        {
            return;
        }

        if (!m_hasSrv && allocateSrv)
        {
            for (UINT i = 0; i < Renderer::SwapChain::BufferCount; ++i)
            {
                allocateSrv(&m_srvCpuHandles[i], &m_srvGpuHandles[i]);
            }
            m_hasSrv = true;
        }

        if (!m_hasSrv)
        {
            return;
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
            0, 1, 2, D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1
        );
        srvDesc.Format                        = Renderer::SwapChain::BackBufferFormat;
        srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip     = 0;
        srvDesc.Texture2D.MipLevels           = 1;
        srvDesc.Texture2D.PlaneSlice          = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

        for (UINT i = 0; i < Renderer::SwapChain::BufferCount; ++i)
        {
            ID3D12Resource* const offscreenResource = m_renderer.GetOffscreenResource(i);
            if (offscreenResource)
            {
                device->CreateShaderResourceView(offscreenResource, &srvDesc, m_srvCpuHandles[i]);
            }
        }
    }

    void ViewportPanel::UpdateCamera(float deltaTime, bool windowFocused, ImGuiContext* imguiContext)
    {
        constexpr double turnSpeed        = 1.0;
        constexpr double baseMoveSpeed    = 120.0;
        constexpr double zoomSpeed        = 120.0;
        constexpr double mouseSensitivity = 0.003;
        constexpr double pitchLimit       = 1.50;

        const double dt = std::clamp(static_cast<double>(deltaTime), 0.0, 0.1);

        Maths::QuatD cameraQuat = Maths::QuatD::FromEulerAngles(m_cameraPitch, m_cameraYaw, 0.0);
        Maths::Vec3D forward    = cameraQuat.Rotate(Maths::Vec3D::Forward());
        Maths::Vec3D right      = cameraQuat.Rotate(Maths::Vec3D::Right());

        if (imguiContext && windowFocused)
        {
            ImGui::SetCurrentContext(imguiContext);
            const ImGuiIO& io = ImGui::GetIO();

            if (!io.WantTextInput && m_isFocused)
            {
                if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
                {
                    m_cameraYaw   += static_cast<double>(io.MouseDelta.x) * mouseSensitivity;
                    m_cameraPitch -= static_cast<double>(io.MouseDelta.y) * mouseSensitivity;
                    m_cameraPitch  = std::clamp(m_cameraPitch, -pitchLimit, pitchLimit);
                }

                // Arrow keys adjust yaw and pitch
                if (ImGui::IsKeyDown(ImGuiKey_LeftArrow))
                {
                    m_cameraYaw -= turnSpeed * dt;
                }
                if (ImGui::IsKeyDown(ImGuiKey_RightArrow))
                {
                    m_cameraYaw += turnSpeed * dt;
                }
                if (ImGui::IsKeyDown(ImGuiKey_UpArrow))
                {
                    m_cameraPitch = std::clamp(m_cameraPitch + turnSpeed * dt, -pitchLimit, pitchLimit);
                }
                if (ImGui::IsKeyDown(ImGuiKey_DownArrow))
                {
                    m_cameraPitch = std::clamp(m_cameraPitch - turnSpeed * dt, -pitchLimit, pitchLimit);
                }

                cameraQuat = Maths::QuatD::FromEulerAngles(m_cameraPitch, m_cameraYaw, 0.0);
                forward    = cameraQuat.Rotate(Maths::Vec3D::Forward());
                right      = cameraQuat.Rotate(Maths::Vec3D::Right());

                // WASD, Space/LeftShift free translation
                Maths::Vec3D moveDelta(0.0, 0.0, 0.0);
                if (ImGui::IsKeyDown(ImGuiKey_W))
                {
                    moveDelta += forward;
                }
                if (ImGui::IsKeyDown(ImGuiKey_S))
                {
                    moveDelta -= forward;
                }
                if (ImGui::IsKeyDown(ImGuiKey_D))
                {
                    moveDelta += right;
                }
                if (ImGui::IsKeyDown(ImGuiKey_A))
                {
                    moveDelta -= right;
                }
                if (ImGui::IsKeyDown(ImGuiKey_Space))
                {
                    moveDelta += Maths::Vec3D::Up();
                }
                if (ImGui::IsKeyDown(ImGuiKey_LeftShift))
                {
                    moveDelta -= Maths::Vec3D::Up();
                }

                if (moveDelta.LengthSquared() > 0.0)
                {
                    m_cameraPosition += moveDelta.Normalised() * (baseMoveSpeed * dt);
                }

                // Bracket keys dolly forward and backward
                if (ImGui::IsKeyDown(ImGuiKey_LeftBracket))
                {
                    m_cameraPosition += forward * (zoomSpeed * dt);
                }
                if (ImGui::IsKeyDown(ImGuiKey_RightBracket))
                {
                    m_cameraPosition -= forward * (zoomSpeed * dt);
                }
            }
        }

        const Maths::Vec3D up = cameraQuat.Rotate(Maths::Vec3D::Up());
        m_camera.SetLookAt(m_cameraPosition, m_cameraPosition + forward, up);
    }

    void ViewportPanel::SetCameraPose(const Maths::Vec3D& position, double yaw, double pitch) noexcept
    {
        constexpr double pitchLimit = 1.50;
        m_cameraPosition = position;
        m_cameraYaw      = yaw;
        m_cameraPitch    = std::clamp(pitch, -pitchLimit, pitchLimit);

        const Maths::QuatD cameraQuat = Maths::QuatD::FromEulerAngles(m_cameraPitch, m_cameraYaw, 0.0);
        const Maths::Vec3D forward    = cameraQuat.Rotate(Maths::Vec3D::Forward());
        const Maths::Vec3D up         = cameraQuat.Rotate(Maths::Vec3D::Up());
        m_camera.SetLookAt(m_cameraPosition, m_cameraPosition + forward, up);
    }

    void ViewportPanel::DrawToolbar()
    {
        const float frameHeight    = ImGui::GetFrameHeight();
        const float toolbarHeight  = frameHeight + 12.0f;
        const float buttonWidth    = frameHeight + 6.0f;
        const float buttonHeight   = frameHeight + 2.0f;
        const float containerRound = 0.0f;
        constexpr int buttonCount  = 3;
        const float groupWidth     = buttonWidth * buttonCount;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.047f, 0.047f, 0.000f, 1.00f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

        if (ImGui::BeginChild("ViewportToolbar", ImVec2(0.0f, toolbarHeight), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 barMin  = ImGui::GetWindowPos();
            const ImVec2 barMax  = ImVec2(barMin.x + ImGui::GetWindowWidth(), barMin.y + toolbarHeight);

            const float startX = std::max(8.0f, (ImGui::GetWindowWidth() - groupWidth) * 0.5f);
            const float startY = (toolbarHeight - buttonHeight) * 0.5f;

            // Encapsulate the three playback controls inside a unified segmented container
            const ImVec2 groupScreenMin(barMin.x + startX, barMin.y + startY);
            const ImVec2 groupScreenMax(groupScreenMin.x + groupWidth, groupScreenMin.y + buttonHeight);
            drawList->AddRectFilled(groupScreenMin, groupScreenMax, IM_COL32(21, 21, 26, 255), containerRound);

            const auto drawToolbarButton = [&](
                const char* id,
                const char* tooltip,
                bool enabled,
                bool active,
                ImDrawFlags cornerFlags,
                ImU32 symbolActiveCol,
                ImU32 symbolIdleCol,
                auto&& drawSymbolFn
            ) -> bool
            {
                const ImVec2 btnMin = ImGui::GetCursorScreenPos();
                const ImVec2 btnMax = ImVec2(btnMin.x + buttonWidth, btnMin.y + buttonHeight);

                bool clicked = false;
                if (enabled)
                {
                    clicked = ImGui::InvisibleButton(id, ImVec2(buttonWidth, buttonHeight));
                }
                else
                {
                    ImGui::Dummy(ImVec2(buttonWidth, buttonHeight));
                }

                const bool hovered = enabled && ImGui::IsItemHovered();
                const bool held    = enabled && ImGui::IsItemActive();

                if (active || held || hovered)
                {
                    ImU32 bgCol = IM_COL32(31, 32, 41, 255);
                    if (active)
                    {
                        bgCol = IM_COL32(38, 42, 54, 255);
                    }
                    else if (held)
                    {
                        bgCol = IM_COL32(42, 44, 56, 255);
                    }
                    drawList->AddRectFilled(btnMin, btnMax, bgCol, containerRound, cornerFlags);
                }

                const ImU32 disabledCol = IM_COL32(82, 85, 96, 255);
                const ImU32 symbolCol   = !enabled ? disabledCol : (active || hovered ? symbolActiveCol : symbolIdleCol);
                drawSymbolFn(drawList, btnMin, buttonWidth, buttonHeight, symbolCol);

                if (hovered)
                {
                    ImGui::SetTooltip("%s", tooltip);
                }

                return clicked;
            };

            const bool isStopped = (m_playState == PlayState::Stopped);
            const bool isPlaying = (m_playState == PlayState::Playing);
            const bool isPaused  = (m_playState == PlayState::Paused);

            ImGui::SetCursorPos(ImVec2(startX, startY));
            if (drawToolbarButton(
                "##ToolbarPlay",
                isPaused ? "Resume" : "Play",
                !isPlaying,
                isPlaying,
                ImDrawFlags_RoundCornersLeft,
                IM_COL32(64, 214, 108, 255),
                IM_COL32(52, 184, 92, 255),
                [](ImDrawList* dl, const ImVec2& min, float width, float height, ImU32 col)
                {
                    const float iconSize = std::min(width, height);
                    const float offsetX  = (width - iconSize) * 0.5f;
                    const float padX     = iconSize * 0.34f;
                    const float padY     = iconSize * 0.27f;
                    const ImVec2 p0(min.x + offsetX + padX, min.y + padY);
                    const ImVec2 p1(min.x + offsetX + padX, min.y + height - padY);
                    const ImVec2 p2(min.x + offsetX + iconSize - padX * 0.80f, min.y + height * 0.50f);
                    dl->AddTriangleFilled(p0, p1, p2, col);
                }
            ))
            {
                m_playState = PlayState::Playing;
                Core::Logger::LogInfo("[Editor] Simulation loop started.");
                if (m_playStateCallback)
                {
                    m_playStateCallback(m_playState);
                }
            }

            ImGui::SameLine(0.0f, 0.0f);
            if (drawToolbarButton(
                "##ToolbarPause",
                "Pause",
                isPlaying,
                isPaused,
                ImDrawFlags_RoundCornersNone,
                IM_COL32(236, 238, 242, 255),
                IM_COL32(205, 208, 214, 255),
                [](ImDrawList* dl, const ImVec2& min, float width, float height, ImU32 col)
                {
                    const float iconSize = std::min(width, height);
                    const float barWidth = std::max(2.0f, iconSize * 0.13f);
                    const float gap      = std::max(2.0f, iconSize * 0.14f);
                    const float padY     = iconSize * 0.28f;
                    const float totalW   = barWidth * 2.0f + gap;
                    const float leftX    = min.x + (width - totalW) * 0.5f;
                    const float rightX   = leftX + barWidth + gap;
                    dl->AddRectFilled(ImVec2(leftX, min.y + padY), ImVec2(leftX + barWidth, min.y + height - padY), col, 1.0f);
                    dl->AddRectFilled(ImVec2(rightX, min.y + padY), ImVec2(rightX + barWidth, min.y + height - padY), col, 1.0f);
                }
            ))
            {
                m_playState = PlayState::Paused;
                Core::Logger::LogInfo("[Editor] Simulation loop paused.");
                if (m_playStateCallback)
                {
                    m_playStateCallback(m_playState);
                }
            }

            ImGui::SameLine(0.0f, 0.0f);
            if (drawToolbarButton(
                "##ToolbarStop",
                "Stop",
                !isStopped,
                false,
                ImDrawFlags_RoundCornersRight,
                IM_COL32(238, 86, 92, 255),
                IM_COL32(204, 70, 76, 255),
                [](ImDrawList* dl, const ImVec2& min, float width, float height, ImU32 col)
                {
                    const float iconSize = std::min(width, height);
                    const float side     = iconSize * 0.40f;
                    const float leftX    = min.x + (width - side) * 0.5f;
                    const float topY     = min.y + (height - side) * 0.5f;
                    dl->AddRectFilled(ImVec2(leftX, topY), ImVec2(leftX + side, topY + side), col, 1.5f);
                }
            ))
            {
                m_playState = PlayState::Stopped;
                Core::Logger::LogInfo("[Editor] Simulation loop stopped.");
                if (m_playStateCallback)
                {
                    m_playStateCallback(m_playState);
                }
            }

            // Internal segment dividers and outer container border
            const ImU32 dividerCol = IM_COL32(38, 38, 46, 255);
            drawList->AddLine(
                ImVec2(groupScreenMin.x + buttonWidth, groupScreenMin.y + 1.0f),
                ImVec2(groupScreenMin.x + buttonWidth, groupScreenMax.y - 1.0f),
                dividerCol
            );
            drawList->AddLine(
                ImVec2(groupScreenMin.x + buttonWidth * 2.0f, groupScreenMin.y + 1.0f),
                ImVec2(groupScreenMin.x + buttonWidth * 2.0f, groupScreenMax.y - 1.0f),
                dividerCol
            );
            drawList->AddRect(groupScreenMin, groupScreenMax, dividerCol, containerRound);

            // Bottom separator line between toolbar and 3D viewport
            drawList->AddLine(
                ImVec2(barMin.x, barMax.y - 1.0f),
                ImVec2(barMax.x, barMax.y - 1.0f),
                dividerCol
            );
        }
        ImGui::EndChild();

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor();
    }

    void ViewportPanel::Draw(EditorContext& context)
    {
        ImGuiWindowClass viewportWindowClass{};
        viewportWindowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
        ImGui::SetNextWindowClass(&viewportWindowClass);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        constexpr ImGuiWindowFlags viewportFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        if (ImGui::Begin(ViewportTitle, nullptr, viewportFlags))
        {
            DrawToolbar();

            const ImVec2 viewportMin = ImGui::GetCursorScreenPos();
            const ImVec2 available   = ImGui::GetContentRegionAvail();
            if (available.x >= 1.0f && available.y >= 1.0f)
            {
                m_desiredWidth  = static_cast<uint32_t>(available.x);
                m_desiredHeight = static_cast<uint32_t>(available.y);

                if (m_hasSrv)
                {
                    const UINT srvIndex = m_renderer.GetOffscreenIndex() % Renderer::SwapChain::BufferCount;
                    ImGui::Image(static_cast<ImTextureID>(m_srvGpuHandles[srvIndex].ptr), available);
                    m_isHovered = ImGui::IsItemHovered();
                    if (m_isHovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
                    {
                        ImGui::SetWindowFocus();
                    }
                    if (m_isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && context.scene)
                    {
                        const ImVec2 mousePos = ImGui::GetMousePos();
                        const float normX = (mousePos.x - viewportMin.x) / available.x;
                        const float normY = (mousePos.y - viewportMin.y) / available.y;
                        const Maths::RayD pickRay = BuildPickRay(m_camera, normX, normY);
                        context.selectedObjectId = PickSceneObject(*context.scene, pickRay);
                    }

                    if (context.scene && context.selectedObjectId != 0)
                    {
                        if (const auto selected = context.scene->FindObject(context.selectedObjectId))
                        {
                            DrawSelectionHighlight(ImGui::GetWindowDrawList(), m_camera, *selected, viewportMin, available);
                        }
                    }
                }
                else
                {
                    m_isHovered = false;
                }
            }
            m_isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
            if (m_isFocused && available.x >= 1.0f && available.y >= 1.0f)
            {
                const ImVec2 viewportMax(viewportMin.x + available.x, viewportMin.y + available.y);
                ImGui::GetWindowDrawList()->AddRect(
                    ImVec2(viewportMin.x + 0.5f, viewportMin.y + 0.5f),
                    ImVec2(viewportMax.x - 0.5f, viewportMax.y - 0.5f),
                    IM_COL32(38, 112, 225, 255),
                    0.0f,
                    0,
                    1.0f
                );
            }
        }
        else
        {
            m_isHovered = false;
            m_isFocused = false;
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
    }
}
