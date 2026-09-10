// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "DxCheck.h"
#include "SwapChain.h"
#include "CommandContext.h"
#include "PipelineState.h"
#include "ConstantBuffer.h"
#include "VertexBuffer.h"
#include "Camera.h"

#include <d3d12.h>
#include <cstdint>

namespace Sandbox3D::Renderer
{
    // Constant buffer layout matching HLSL cbuffer ModelViewProjectionBuffer
    struct ModelViewProjectionBuffer
    {
        Maths::Mat4x4 mvp;
    };

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
            uint32_t height
        );

        void Shutdown(ID3D12CommandQueue* commandQueue) noexcept;
        void OnResize(ID3D12Device* device, ID3D12CommandQueue* commandQueue, uint32_t width, uint32_t height);
        void Render(ID3D12CommandQueue* commandQueue);

        [[nodiscard]] Camera& GetCamera() noexcept { return m_camera; }
        [[nodiscard]] const Camera& GetCamera() const noexcept { return m_camera; }

    private:
        void UpdateViewportAndScissor(uint32_t width, uint32_t height);

    private:
        SwapChain                                   m_swapChain;
        CommandContext                              m_commandContext;
        PipelineState                               m_pipelineState;
        ConstantBuffer<ModelViewProjectionBuffer>   m_mvpConstantBuffer;
        VertexBuffer                                m_triangleVertexBuffer;
        Camera                                      m_camera;

        D3D12_VIEWPORT                              m_viewport{};
        D3D12_RECT                                  m_scissorRect{};
        uint32_t                                    m_width{ 0 };
        uint32_t                                    m_height{ 0 };
        bool                                        m_isInitialised{ false };
    };
}

