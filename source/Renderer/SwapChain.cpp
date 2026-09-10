// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "SwapChain.h"

#include <algorithm>
#include <stdexcept>

namespace Sandbox3D::Renderer
{
    SwapChain::~SwapChain()
    {
        for (UINT i = 0; i < BufferCount; ++i)
        {
            m_renderTargets[i].Reset();
        }
        m_rtvHeap.Reset();
        m_swapChain.Reset();
    }

    void SwapChain::Initialise(
        IDXGIFactory6* factory,
        ID3D12Device* device,
        ID3D12CommandQueue* commandQueue,
        HWND hwnd,
        uint32_t width,
        uint32_t height
    )
    {
        m_width  = std::max(width, 1u);
        m_height = std::max(height, 1u);

        // Check for variable refresh rate / tearing support
        BOOL allowTearing = FALSE;
        if (SUCCEEDED(factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))))
        {
            m_tearingSupported = (allowTearing == TRUE);
        }

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width              = m_width;
        swapChainDesc.Height             = m_height;
        swapChainDesc.Format             = BackBufferFormat;
        swapChainDesc.Stereo             = FALSE;
        swapChainDesc.SampleDesc.Count   = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount        = BufferCount;
        swapChainDesc.Scaling            = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode          = DXGI_ALPHA_MODE_UNSPECIFIED;
        swapChainDesc.Flags              = m_tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

        ComPtr<IDXGISwapChain1> swapChain1;
        HR_CHECK(factory->CreateSwapChainForHwnd(
            commandQueue,
            hwnd,
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain1
        ));

        // Disable standard Alt+Enter windowed-to-fullscreen handling
        HR_CHECK(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));

        HR_CHECK(swapChain1.As(&m_swapChain));
        m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

        CreateRtvDescriptorHeap(device);
        CreateRenderTargetViews(device);
    }

    void SwapChain::CreateRtvDescriptorHeap(ID3D12Device* device)
    {
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = BufferCount;
        rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        HR_CHECK(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
        m_rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    void SwapChain::CreateRenderTargetViews(ID3D12Device* device)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

        for (UINT i = 0; i < BufferCount; ++i)
        {
            HR_CHECK(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
            device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
            rtvHandle.ptr += m_rtvDescriptorSize;
        }
    }

    void SwapChain::Resize(ID3D12Device* device, uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
        {
            return;
        }

        m_width  = width;
        m_height = height;

        // Release references to existing back buffers before resizing swap chain
        for (UINT i = 0; i < BufferCount; ++i)
        {
            m_renderTargets[i].Reset();
        }

        const UINT flags = m_tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
        HR_CHECK(m_swapChain->ResizeBuffers(
            BufferCount,
            m_width,
            m_height,
            BackBufferFormat,
            flags
        ));

        m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
        CreateRenderTargetViews(device);
    }

    void SwapChain::Present(bool vSync)
    {
        const UINT syncInterval = vSync ? 1 : 0;
        const UINT presentFlags = (!vSync && m_tearingSupported) ? DXGI_PRESENT_ALLOW_TEARING : 0;

        HR_CHECK(m_swapChain->Present(syncInterval, presentFlags));
        m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
    }

    ID3D12Resource* SwapChain::GetCurrentRenderTarget() const noexcept
    {
        return m_renderTargets[m_currentBackBufferIndex].Get();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE SwapChain::GetCurrentRtvHandle() const noexcept
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(m_currentBackBufferIndex) * m_rtvDescriptorSize;
        return handle;
    }
}

