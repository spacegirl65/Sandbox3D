// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "DxCheck.h"
#include "Maths/Maths.h"

#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <algorithm>

namespace Sandbox3D::Renderer
{
    using Microsoft::WRL::ComPtr;

    // Encapsulates off-screen multisampled render targets, depth-stencil buffers, views, and resolve passes
    class FrameBuffer final
    {
    public:
        FrameBuffer() = default;
        ~FrameBuffer() = default;

        FrameBuffer(const FrameBuffer&) = delete;
        FrameBuffer& operator=(const FrameBuffer&) = delete;
        FrameBuffer(FrameBuffer&&) noexcept = default;
        FrameBuffer& operator=(FrameBuffer&&) noexcept = default;

        // Configures render target texture, depth-stencil buffer, and descriptor heaps
        void Initialise(
            ID3D12Device* device,
            uint32_t width,
            uint32_t height,
            DXGI_FORMAT rtvFormat,
            DXGI_FORMAT dsvFormat = DXGI_FORMAT_D32_FLOAT,
            uint32_t sampleCount = 4,
            const Maths::Vec4& clearColor = Maths::Vec4(0.718f, 0.865f, 0.986f, 1.0f)
        );

        // Releases GPU resources and descriptor heaps
        void Shutdown() noexcept;

        // Resizes off-screen targets and viewports to match window dimensions
        void Resize(ID3D12Device* device, uint32_t width, uint32_t height);

        // Pipeline binding operations
        void Bind(ID3D12GraphicsCommandList* commandList) const noexcept;
        void BindDepthOnly(ID3D12GraphicsCommandList* commandList) const noexcept;

        // Clear operations
        void Clear(ID3D12GraphicsCommandList* commandList) const noexcept;
        void ClearRenderTarget(ID3D12GraphicsCommandList* commandList) const noexcept;
        void ClearRenderTarget(ID3D12GraphicsCommandList* commandList, const Maths::Vec4& color) const noexcept;
        void ClearDepth(ID3D12GraphicsCommandList* commandList, float depth = 0.0f, uint8_t stencil = 0) const noexcept;
        void ClearDepthScissor(ID3D12GraphicsCommandList* commandList, const D3D12_RECT& scissorRect, float depth = 0.0f) const noexcept;

        // Hardware MSAA resolve or blit to the destination swap chain back buffer
        void Resolve(
            ID3D12GraphicsCommandList* commandList,
            ID3D12Resource* destinationBackBuffer,
            D3D12_RESOURCE_STATES destinationCurrentState = D3D12_RESOURCE_STATE_PRESENT
        ) const noexcept;

        // Clear colour configuration
        void SetClearColor(const Maths::Vec4& color) noexcept { m_clearColor = color; }
        [[nodiscard]] const Maths::Vec4& GetClearColor() const noexcept { return m_clearColor; }

        // Metrics and state accessors
        [[nodiscard]] bool IsInitialised() const noexcept { return m_isInitialised; }
        [[nodiscard]] uint32_t GetWidth() const noexcept { return m_width; }
        [[nodiscard]] uint32_t GetHeight() const noexcept { return m_height; }
        [[nodiscard]] uint32_t GetSampleCount() const noexcept { return m_sampleCount; }
        [[nodiscard]] uint32_t GetMsaaQualityLevels() const noexcept { return m_msaaQualityLevels; }
        [[nodiscard]] DXGI_FORMAT GetRtvFormat() const noexcept { return m_rtvFormat; }
        [[nodiscard]] DXGI_FORMAT GetDsvFormat() const noexcept { return m_dsvFormat; }

        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetRtvHandle() const noexcept;
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetDsvHandle() const noexcept;
        [[nodiscard]] ID3D12Resource* GetRenderTarget() const noexcept { return m_renderTarget.Get(); }
        [[nodiscard]] ID3D12Resource* GetDepthStencil() const noexcept { return m_depthStencil.Get(); }

        [[nodiscard]] const D3D12_VIEWPORT& GetViewport() const noexcept { return m_viewport; }
        [[nodiscard]] const D3D12_RECT& GetScissorRect() const noexcept { return m_scissorRect; }
        [[nodiscard]] const Maths::Rect& GetViewportRect() const noexcept { return m_viewportRect; }

    private:
        void CreateRenderTarget(ID3D12Device* device, uint32_t width, uint32_t height);
        void CreateDepthStencil(ID3D12Device* device, uint32_t width, uint32_t height);
        void UpdateViewportAndScissor(uint32_t width, uint32_t height);
        void CheckMsaaSupport(ID3D12Device* device);

    private:
        ComPtr<ID3D12Resource>       m_renderTarget;
        ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
        ComPtr<ID3D12Resource>       m_depthStencil;
        ComPtr<ID3D12DescriptorHeap> m_dsvHeap;

        D3D12_VIEWPORT               m_viewport{};
        D3D12_RECT                   m_scissorRect{};
        Maths::Rect                  m_viewportRect{};
        Maths::Vec4                  m_clearColor{ 0.718f, 0.865f, 0.986f, 1.0f };

        uint32_t                     m_width{ 0 };
        uint32_t                     m_height{ 0 };
        uint32_t                     m_sampleCount{ 4 };
        uint32_t                     m_msaaQualityLevels{ 0 };
        DXGI_FORMAT                  m_rtvFormat{ DXGI_FORMAT_R8G8B8A8_UNORM };
        DXGI_FORMAT                  m_dsvFormat{ DXGI_FORMAT_D32_FLOAT };
        bool                         m_isInitialised{ false };
    };
}

