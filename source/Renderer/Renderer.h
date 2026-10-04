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
#include <string>
#include <unordered_map>

#include "SceneConstantBuffer.h"
#include "TextOverlay.h"
#include "RenderBatch.h"
#include "DynamicUploadBuffer.h"
#include "FrameBuffer.h"
#include "Texture.h"

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
        void SetTargetUps(float targetUps) noexcept { m_targetUps = (targetUps > 0.0f) ? targetUps : 60.0f; }

        // Camera binding (non-owning pointer interface)
        void SetCamera(Camera* camera) noexcept
        {
            m_camera = camera ? camera : &m_fallbackCamera;
            if (m_width > 0 && m_height > 0)
            {
                m_camera->UpdateAspectRatio(static_cast<float>(m_width) / static_cast<float>(m_height));
            }
        }

        // Scene ambient lighting configuration
        void SetAmbientColor(const Maths::Vec4& ambient) noexcept { m_ambientColor = ambient; }

        // Clear colour (background and sky) configuration
        void SetClearColor(const Maths::Vec4& color) noexcept
        {
            m_clearColor = color;
            m_frameBuffer.SetClearColor(color);
        }

        [[nodiscard]] const FrameBuffer& GetFrameBuffer() const noexcept { return m_frameBuffer; }
        [[nodiscard]] FrameBuffer& GetFrameBuffer() noexcept { return m_frameBuffer; }

        // Atmospheric aerial perspective and fog configuration
        void SetFogColour(const Maths::Vec4& color) noexcept { m_fogColor = color; }
        void SetFogParams(const Maths::Vec4& params) noexcept { m_fogParams = params; }
        void SetFogParams(float start, float end, float density) noexcept { m_fogParams = Maths::Vec4(start, end, density, 0.0f); }

        // Orientation gizmo management
        void SetShowGizmo(bool show) noexcept { m_showGizmo = show; }

        // Diagnostic text overlay management
        void SetShowOverlay(bool show) noexcept
        {
            m_showOverlay = show;
            if (m_textOverlay)
            {
                m_textOverlay->SetVisible(show);
            }
        }
        void ToggleOverlay() noexcept { SetShowOverlay(!m_showOverlay); }

        // Depth pre-pass configuration
        void SetEnableDepthPrePass(bool enable) noexcept { m_enableDepthPrePass = enable; }
        [[nodiscard]] bool IsDepthPrePassEnabled() const noexcept { return m_enableDepthPrePass; }

        // Alternative terrain textures configuration
        void SetEnableAlternativeTextures(bool enable) noexcept { m_enableAlternativeTextures = enable; }
        [[nodiscard]] bool IsAlternativeTexturesEnabled() const noexcept { return m_enableAlternativeTextures; }

        // Pre-baked LiDAR horizon occlusion, crevice depth, and directional horizon angle maps
        void SetLidarOcclusionMaps(
            ID3D12Device* device,
            ID3D12CommandQueue* commandQueue,
            const void* aoPixelData,
            const void* horizon0PixelData,
            const void* horizon1PixelData,
            uint32_t width,
            uint32_t height
        );

        void SetLidarOcclusionMap(
            ID3D12Device* device,
            ID3D12CommandQueue* commandQueue,
            const void* pixelData,
            uint32_t width,
            uint32_t height
        );

    private:
        void InitialiseTextureResources(ID3D12Device* device, ID3D12CommandQueue* commandQueue);

        // Pipeline state & material shader lookup
        [[nodiscard]] PipelineState* GetPipelineState(const std::string& shaderName) noexcept;
        [[nodiscard]] const PipelineState* GetPipelineState(const std::string& shaderName) const noexcept;

    private:
        SwapChain                                   m_swapChain;
        CommandContext                              m_commandContext;
        FrameBuffer                                 m_frameBuffer;
        ComPtr<ID3D12DescriptorHeap>                m_srvHeap;
        uint32_t                                    m_srvDescriptorSize{ 0 };
        std::vector<std::shared_ptr<Texture>>       m_terrainTextures;
        std::shared_ptr<Texture>                    m_lidarOcclusionTexture;
        std::shared_ptr<Texture>                    m_lidarHorizonTexture0;
        std::shared_ptr<Texture>                    m_lidarHorizonTexture1;
        std::unordered_map<std::string, PipelineState> m_pipelineStates;
        PipelineState                               m_pipelineState;
        PipelineState                               m_unlitPipelineState;
        PipelineState                               m_depthPipelineState;
        RenderQueue                                 m_renderQueue;
        DynamicUploadBuffer                         m_dynamicConstantBuffer;
        std::shared_ptr<Mesh>                       m_gizmoMesh;
        std::unique_ptr<TextOverlay>                m_textOverlay;

        Camera                                      m_fallbackCamera{};
        Camera*                                     m_camera{ &m_fallbackCamera };

        Maths::Vec4                                 m_clearColor{ 0.718f, 0.865f, 0.986f, 1.0f };
        Maths::Vec4                                 m_fogColor{ 0.718f, 0.865f, 0.986f, 1.0f };
        Maths::Vec4                                 m_fogParams{ 120.0f, 1600.0f, 0.0010f, 0.0f };
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
        float                                       m_gizmoSize{ 122.0f };
        float                                       m_gizmoMarginX{ 24.0f };
        float                                       m_gizmoMarginY{ 16.0f };
        uint32_t                                    m_width{ 0 };
        uint32_t                                    m_height{ 0 };
        bool                                        m_showGizmo{ true };
        bool                                        m_showOverlay{ false };
        bool                                        m_enableDepthPrePass{ true };
        bool                                        m_enableAlternativeTextures{ false };
        bool                                        m_isInitialised{ false };
    };
}
