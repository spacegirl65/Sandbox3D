// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "DxCheck.h"
#include "SwapChain.h"
#include "CommandContext.h"
#include "PipelineState.h"
#include "ConstantBuffer.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Mesh.h"
#include "RenderItem.h"
#include "Engine/Camera.h"

#include <d3d12.h>
#include <cstdint>
#include <span>
#include <vector>
#include <memory>

#include "SceneConstantBuffer.h"
#include "TextOverlay.h"

namespace Sandbox3D::Renderer
{
    using Engine::Camera;
    using Maths::Vec4;
    using Microsoft::WRL::ComPtr;

    // Orchestrates rendering passes, pipeline execution, and frame presentations
    class Renderer final
    {
    public:
        Renderer() = default;
        ~Renderer() = default;

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;
        Renderer(Renderer&&) noexcept = default;
        Renderer& operator=(Renderer&&) noexcept = default;

        void Initialise(
            IDXGIFactory6* factory,
            ID3D12Device* device,
            ID3D12CommandQueue* commandQueue,
            HWND hwnd,
            uint32_t width,
            uint32_t height,
            const std::wstring& gpuDescription = {}
        );

        void Shutdown(ID3D12CommandQueue* commandQueue) noexcept;
        void OnResize(ID3D12Device* device, ID3D12CommandQueue* commandQueue, uint32_t width, uint32_t height);
        void Render(
            ID3D12CommandQueue* commandQueue,
            std::span<const RenderItem> renderItems = {},
            std::span<const GpuLight> lights = {},
            bool vSync = true,
            size_t totalSceneItems = 0,
            float updatesPerSecond = 60.0f,
            float targetUps = 60.0f
        );

        void SetTargetFps(float targetFps) noexcept { m_targetFps = (targetFps > 0.0f) ? targetFps : 120.0f; }
        [[nodiscard]] float GetTargetFps() const noexcept { return m_targetFps; }

        void SetTargetUps(float targetUps) noexcept { m_targetUps = (targetUps > 0.0f) ? targetUps : 60.0f; }
        [[nodiscard]] float GetTargetUps() const noexcept { return m_targetUps; }

        // Camera binding (non-owning pointer/reference interface)
        [[nodiscard]] Camera& GetCamera() noexcept { return *m_camera; }
        [[nodiscard]] const Camera& GetCamera() const noexcept { return *m_camera; }
        void SetCamera(Camera* camera) noexcept
        {
            m_camera = camera ? camera : &m_fallbackCamera;
            if (m_width > 0 && m_height > 0)
            {
                m_camera->UpdateAspectRatio(static_cast<float>(m_width) / static_cast<float>(m_height));
            }
        }
        void SetCamera(Camera& camera) noexcept
        {
            m_camera = &camera;
            if (m_width > 0 && m_height > 0)
            {
                m_camera->UpdateAspectRatio(static_cast<float>(m_width) / static_cast<float>(m_height));
            }
        }

        // Directional and scene ambient lighting configuration
        void SetDirectionalLight(
            const Maths::Vec3& direction,
            const Maths::Vec4& color = Maths::Vec4(1.0f, 1.0f, 1.0f, 1.0f),
            const Maths::Vec4& ambient = Maths::Vec4(0.2f, 0.2f, 0.25f, 1.0f)
        ) noexcept
        {
            const Maths::Vec3 normDir = direction.Normalised();
            m_defaultLight.direction   = Maths::Vec4(normDir.x, normDir.y, normDir.z, 0.0f);
            m_defaultLight.color       = color;
            m_defaultLight.attenuation = Maths::Vec4(1.0f, 0.0f, 0.0f, 0.0f);
            m_ambientColor             = ambient;
        }

        [[nodiscard]] const Maths::Vec4& GetLightDirection() const noexcept { return m_defaultLight.direction; }
        [[nodiscard]] const Maths::Vec4& GetLightColor() const noexcept { return m_defaultLight.color; }
        [[nodiscard]] const Maths::Vec4& GetAmbientColor() const noexcept { return m_ambientColor; }
        void SetAmbientColor(const Maths::Vec4& ambient) noexcept { m_ambientColor = ambient; }

        // Clear color (background / sky) configuration
        void SetClearColor(const Maths::Vec4& color) noexcept { m_clearColor = color; }
        [[nodiscard]] const Maths::Vec4& GetClearColor() const noexcept { return m_clearColor; }

        // Orientation gizmo management
        void SetShowGizmo(bool show) noexcept { m_showGizmo = show; }
        [[nodiscard]] bool IsGizmoVisible() const noexcept { return m_showGizmo; }
        void SetGizmoSize(float size) noexcept { m_gizmoSize = size; }
        [[nodiscard]] float GetGizmoSize() const noexcept { return m_gizmoSize; }
        void SetGizmoMargin(float margin) noexcept { m_gizmoMarginX = margin; m_gizmoMarginY = margin; }
        void SetGizmoMargin(float marginX, float marginY) noexcept { m_gizmoMarginX = marginX; m_gizmoMarginY = marginY; }
        void SetGizmoMarginX(float marginX) noexcept { m_gizmoMarginX = marginX; }
        void SetGizmoMarginY(float marginY) noexcept { m_gizmoMarginY = marginY; }
        [[nodiscard]] float GetGizmoMargin() const noexcept { return m_gizmoMarginX; }
        [[nodiscard]] float GetGizmoMarginX() const noexcept { return m_gizmoMarginX; }
        [[nodiscard]] float GetGizmoMarginY() const noexcept { return m_gizmoMarginY; }

        // Diagnostic text overlay management
        void SetShowOverlay(bool show) noexcept
        {
            m_showOverlay = show;
            if (m_textOverlay)
            {
                m_textOverlay->SetVisible(show);
            }
        }
        [[nodiscard]] bool IsOverlayVisible() const noexcept { return m_showOverlay; }
        void ToggleOverlay() noexcept { SetShowOverlay(!m_showOverlay); }
        [[nodiscard]] TextOverlay* GetTextOverlay() noexcept { return m_textOverlay.get(); }
        [[nodiscard]] const TextOverlay* GetTextOverlay() const noexcept { return m_textOverlay.get(); }

        // Anti-aliasing configuration
        [[nodiscard]] uint32_t GetSampleCount() const noexcept { return m_sampleCount; }
        [[nodiscard]] bool IsMsaaEnabled() const noexcept { return m_sampleCount > 1; }

    private:
        void UpdateViewportAndScissor(uint32_t width, uint32_t height);
        void CheckMsaaSupport(ID3D12Device* device);
        void CreateMsaaRenderTarget(ID3D12Device* device, uint32_t width, uint32_t height);
        void CreateDepthStencil(ID3D12Device* device, uint32_t width, uint32_t height);

    private:
        SwapChain                                   m_swapChain;
        CommandContext                              m_commandContext;
        PipelineState                               m_pipelineState;
        PipelineState                               m_unlitPipelineState;
        ConstantBuffer<SceneConstantBuffer>         m_sceneConstantBuffer;
        ConstantBuffer<SceneConstantBuffer>         m_gizmoConstantBuffer;
        ComPtr<ID3D12Resource>                      m_msaaRenderTarget;
        ComPtr<ID3D12DescriptorHeap>                m_msaaRtvHeap;
        ComPtr<ID3D12Resource>                      m_depthStencilBuffer;
        ComPtr<ID3D12DescriptorHeap>                m_dsvHeap;
        std::shared_ptr<Mesh>                       m_gizmoMesh;
        std::unique_ptr<TextOverlay>                m_textOverlay;

        Camera                                      m_fallbackCamera{};
        Camera*                                     m_camera{ &m_fallbackCamera };

        D3D12_VIEWPORT                              m_viewport{};
        D3D12_RECT                                  m_scissorRect{};
        Maths::Rect                                 m_viewportRect{};
        Maths::Vec4                                 m_clearColor{ 0.718f, 0.865f, 0.986f, 1.0f };
        GpuLight                                    m_defaultLight{
            Maths::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
            Maths::Vec4(-0.577f, -0.707f, -0.408f, 0.0f),
            Maths::Vec4(0.9f, 0.9f, 0.95f, 1.0f),
            Maths::Vec4(1.0f, 0.0f, 0.0f, 0.0f)
        };
        Maths::Vec4                                 m_ambientColor{ 0.2f, 0.2f, 0.25f, 1.0f };
        std::string                                 m_gpuName{};
        std::chrono::high_resolution_clock::time_point m_lastFrameTime{};
        float                                       m_targetFps{ 120.0f };
        float                                       m_targetUps{ 60.0f };
        float                                       m_smoothedFps{ 120.0f };
        float                                       m_smoothedFrameTimeMs{ 8.33f };
        float                                       m_fpsTimeAccumulator{ 0.0f };
        uint32_t                                    m_fpsFrameCount{ 0 };
        float                                       m_gizmoSize{ 112.0f };
        float                                       m_gizmoMarginX{ 24.0f };
        float                                       m_gizmoMarginY{ 16.0f };
        uint32_t                                    m_width{ 0 };
        uint32_t                                    m_height{ 0 };
        uint32_t                                    m_sampleCount{ 4 };
        uint32_t                                    m_msaaQualityLevels{ 0 };
        bool                                        m_showGizmo{ true };
        bool                                        m_showOverlay{ false };
        bool                                        m_isInitialised{ false };
    };
}
