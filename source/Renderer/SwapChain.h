// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "DxCheck.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdint>

namespace Sandbox3D::Renderer
{
    using Microsoft::WRL::ComPtr;

    // Encapsulates DXGI Swap Chain with low-latency modern flip presentation and RTV descriptor heap management
    class SwapChain final
    {
    public:
        static constexpr UINT BufferCount = 2;
        static constexpr DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

        SwapChain() = default;
        ~SwapChain();

        SwapChain(const SwapChain&) = delete;
        SwapChain& operator=(const SwapChain&) = delete;
        SwapChain(SwapChain&&) noexcept = delete;
        SwapChain& operator=(SwapChain&&) noexcept = delete;

        void Initialise(
            IDXGIFactory6* factory,
            ID3D12Device* device,
            ID3D12CommandQueue* commandQueue,
            HWND hwnd,
            uint32_t width,
            uint32_t height
        );

        void Resize(ID3D12Device* device, uint32_t width, uint32_t height);
        void Present(bool vSync = true);

        [[nodiscard]] ID3D12Resource* GetCurrentRenderTarget() const noexcept;
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRtvHandle() const noexcept;
        [[nodiscard]] UINT GetCurrentBackBufferIndex() const noexcept { return m_currentBackBufferIndex; }
        [[nodiscard]] DXGI_FORMAT GetFormat() const noexcept { return BackBufferFormat; }

    private:
        void CreateRtvDescriptorHeap(ID3D12Device* device);
        void CreateRenderTargetViews(ID3D12Device* device);

    private:
        ComPtr<IDXGISwapChain4>         m_swapChain;
        ComPtr<ID3D12DescriptorHeap>    m_rtvHeap;
        ComPtr<ID3D12Resource>          m_renderTargets[BufferCount];
        UINT                            m_rtvDescriptorSize{ 0 };
        UINT                            m_currentBackBufferIndex{ 0 };
        uint32_t                        m_width{ 0 };
        uint32_t                        m_height{ 0 };
        bool                            m_tearingSupported{ false };
    };
}

