// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "FrameBuffer.h"

#include <iostream>

namespace Sandbox3D::Renderer
{
    void FrameBuffer::Initialise(
        ID3D12Device* device,
        uint32_t width,
        uint32_t height,
        DXGI_FORMAT rtvFormat,
        DXGI_FORMAT dsvFormat,
        uint32_t sampleCount,
        const Maths::Vec4& clearColor
    )
    {
        m_width       = std::max(width, 1u);
        m_height      = std::max(height, 1u);
        m_rtvFormat   = rtvFormat;
        m_dsvFormat   = dsvFormat;
        m_sampleCount = sampleCount;
        m_clearColor  = clearColor;

        CheckMsaaSupport(device);
        CreateRenderTarget(device, m_width, m_height);
        CreateDepthStencil(device, m_width, m_height);
        UpdateViewportAndScissor(m_width, m_height);

        m_isInitialised = true;
    }

    void FrameBuffer::Shutdown() noexcept
    {
        if (!m_isInitialised)
        {
            return;
        }

        m_renderTarget.Reset();
        m_rtvHeap.Reset();
        m_depthStencil.Reset();
        m_dsvHeap.Reset();

        m_width         = 0;
        m_height        = 0;
        m_isInitialised = false;
    }

    void FrameBuffer::Resize(ID3D12Device* device, uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
        {
            return;
        }

        m_width  = width;
        m_height = height;

        CreateRenderTarget(device, m_width, m_height);
        CreateDepthStencil(device, m_width, m_height);
        UpdateViewportAndScissor(m_width, m_height);
    }

    void FrameBuffer::Bind(ID3D12GraphicsCommandList* commandList) const noexcept
    {
        if (!commandList || !m_rtvHeap || !m_dsvHeap)
        {
            return;
        }

        const D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetRtvHandle();
        const D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetDsvHandle();
        commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        commandList->RSSetViewports(1, &m_viewport);
        commandList->RSSetScissorRects(1, &m_scissorRect);
    }

    void FrameBuffer::BindDepthOnly(ID3D12GraphicsCommandList* commandList) const noexcept
    {
        if (!commandList || !m_dsvHeap)
        {
            return;
        }

        const D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetDsvHandle();
        commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
        commandList->RSSetViewports(1, &m_viewport);
        commandList->RSSetScissorRects(1, &m_scissorRect);
    }

    void FrameBuffer::Clear(ID3D12GraphicsCommandList* commandList) const noexcept
    {
        ClearRenderTarget(commandList);
        ClearDepth(commandList);
    }

    void FrameBuffer::ClearRenderTarget(ID3D12GraphicsCommandList* commandList) const noexcept
    {
        ClearRenderTarget(commandList, m_clearColor);
    }

    void FrameBuffer::ClearRenderTarget(ID3D12GraphicsCommandList* commandList, const Maths::Vec4& color) const noexcept
    {
        if (!commandList || !m_rtvHeap)
        {
            return;
        }

        const float clearColor[4] = { color.r(), color.g(), color.b(), color.a() };
        commandList->ClearRenderTargetView(GetRtvHandle(), clearColor, 0, nullptr);
    }

    void FrameBuffer::ClearDepth(ID3D12GraphicsCommandList* commandList, float depth, uint8_t stencil) const noexcept
    {
        if (!commandList || !m_dsvHeap)
        {
            return;
        }

        commandList->ClearDepthStencilView(GetDsvHandle(), D3D12_CLEAR_FLAG_DEPTH, depth, stencil, 0, nullptr);
    }

    void FrameBuffer::ClearDepthScissor(ID3D12GraphicsCommandList* commandList, const D3D12_RECT& scissorRect, float depth) const noexcept
    {
        if (!commandList || !m_dsvHeap)
        {
            return;
        }

        commandList->ClearDepthStencilView(GetDsvHandle(), D3D12_CLEAR_FLAG_DEPTH, depth, 0, 1, &scissorRect);
    }

    void FrameBuffer::Resolve(
        ID3D12GraphicsCommandList* commandList,
        ID3D12Resource* destinationBackBuffer,
        D3D12_RESOURCE_STATES destinationCurrentState
    ) const noexcept
    {
        if (!commandList || !destinationBackBuffer || !m_renderTarget)
        {
            return;
        }

        if (m_sampleCount > 1)
        {
            // Transition off-screen MSAA render target to resolve source and destination back buffer to resolve dest
            D3D12_RESOURCE_BARRIER preResolveBarriers[2] = {};
            preResolveBarriers[0].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            preResolveBarriers[0].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            preResolveBarriers[0].Transition.pResource   = m_renderTarget.Get();
            preResolveBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            preResolveBarriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
            preResolveBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            preResolveBarriers[1].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            preResolveBarriers[1].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            preResolveBarriers[1].Transition.pResource   = destinationBackBuffer;
            preResolveBarriers[1].Transition.StateBefore = destinationCurrentState;
            preResolveBarriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_RESOLVE_DEST;
            preResolveBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            commandList->ResourceBarrier(2, preResolveBarriers);

            // Hardware multisample resolve pass into swap chain back buffer
            commandList->ResolveSubresource(
                destinationBackBuffer, 0,
                m_renderTarget.Get(), 0,
                m_rtvFormat
            );

            // Transition destination back buffer back to original state and render target back to render target state
            D3D12_RESOURCE_BARRIER postResolveBarriers[2] = {};
            postResolveBarriers[0].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            postResolveBarriers[0].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            postResolveBarriers[0].Transition.pResource   = destinationBackBuffer;
            postResolveBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RESOLVE_DEST;
            postResolveBarriers[0].Transition.StateAfter  = destinationCurrentState;
            postResolveBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            postResolveBarriers[1].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            postResolveBarriers[1].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            postResolveBarriers[1].Transition.pResource   = m_renderTarget.Get();
            postResolveBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
            postResolveBarriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
            postResolveBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            commandList->ResourceBarrier(2, postResolveBarriers);
        }
        else
        {
            // Non-multisampled direct texture copy
            D3D12_RESOURCE_BARRIER preCopyBarriers[2] = {};
            preCopyBarriers[0].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            preCopyBarriers[0].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            preCopyBarriers[0].Transition.pResource   = m_renderTarget.Get();
            preCopyBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            preCopyBarriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE;
            preCopyBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            preCopyBarriers[1].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            preCopyBarriers[1].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            preCopyBarriers[1].Transition.pResource   = destinationBackBuffer;
            preCopyBarriers[1].Transition.StateBefore = destinationCurrentState;
            preCopyBarriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
            preCopyBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            commandList->ResourceBarrier(2, preCopyBarriers);

            commandList->CopyResource(destinationBackBuffer, m_renderTarget.Get());

            D3D12_RESOURCE_BARRIER postCopyBarriers[2] = {};
            postCopyBarriers[0].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            postCopyBarriers[0].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            postCopyBarriers[0].Transition.pResource   = destinationBackBuffer;
            postCopyBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            postCopyBarriers[0].Transition.StateAfter  = destinationCurrentState;
            postCopyBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            postCopyBarriers[1].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            postCopyBarriers[1].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            postCopyBarriers[1].Transition.pResource   = m_renderTarget.Get();
            postCopyBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
            postCopyBarriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
            postCopyBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            commandList->ResourceBarrier(2, postCopyBarriers);
        }
    }

    D3D12_CPU_DESCRIPTOR_HANDLE FrameBuffer::GetRtvHandle() const noexcept
    {
        return m_rtvHeap ? m_rtvHeap->GetCPUDescriptorHandleForHeapStart() : D3D12_CPU_DESCRIPTOR_HANDLE{};
    }

    D3D12_CPU_DESCRIPTOR_HANDLE FrameBuffer::GetDsvHandle() const noexcept
    {
        return m_dsvHeap ? m_dsvHeap->GetCPUDescriptorHandleForHeapStart() : D3D12_CPU_DESCRIPTOR_HANDLE{};
    }

    void FrameBuffer::CreateRenderTarget(ID3D12Device* device, uint32_t width, uint32_t height)
    {
        m_renderTarget.Reset();

        if (!m_rtvHeap)
        {
            D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
            rtvHeapDesc.NumDescriptors = 1;
            rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            HR_CHECK(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
        }

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type                 = D3D12_HEAP_TYPE_DEFAULT;
        heapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask     = 1;
        heapProps.VisibleNodeMask      = 1;

        D3D12_RESOURCE_DESC rtvDesc = {};
        rtvDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        rtvDesc.Alignment          = 0;
        rtvDesc.Width              = std::max(width, 1u);
        rtvDesc.Height             = std::max(height, 1u);
        rtvDesc.DepthOrArraySize   = 1;
        rtvDesc.MipLevels          = 1;
        rtvDesc.Format             = m_rtvFormat;
        rtvDesc.SampleDesc.Count   = m_sampleCount;
        rtvDesc.SampleDesc.Quality = 0;
        rtvDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        rtvDesc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        const float clearColor[4] = { m_clearColor.r(), m_clearColor.g(), m_clearColor.b(), m_clearColor.a() };
        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format   = m_rtvFormat;
        clearValue.Color[0] = clearColor[0];
        clearValue.Color[1] = clearColor[1];
        clearValue.Color[2] = clearColor[2];
        clearValue.Color[3] = clearColor[3];

        HR_CHECK(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &rtvDesc,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            &clearValue,
            IID_PPV_ARGS(&m_renderTarget)
        ));

        D3D12_RENDER_TARGET_VIEW_DESC rtvViewDesc = {};
        rtvViewDesc.Format        = m_rtvFormat;
        rtvViewDesc.ViewDimension = (m_sampleCount > 1) ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D;

        device->CreateRenderTargetView(m_renderTarget.Get(), &rtvViewDesc, m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    }

    void FrameBuffer::CreateDepthStencil(ID3D12Device* device, uint32_t width, uint32_t height)
    {
        m_depthStencil.Reset();

        if (!m_dsvHeap)
        {
            D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
            dsvHeapDesc.NumDescriptors = 1;
            dsvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            dsvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            HR_CHECK(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));
        }

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type                 = D3D12_HEAP_TYPE_DEFAULT;
        heapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask     = 1;
        heapProps.VisibleNodeMask      = 1;

        D3D12_RESOURCE_DESC depthDesc = {};
        depthDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthDesc.Alignment          = 0;
        depthDesc.Width              = std::max(width, 1u);
        depthDesc.Height             = std::max(height, 1u);
        depthDesc.DepthOrArraySize   = 1;
        depthDesc.MipLevels          = 1;
        depthDesc.Format             = m_dsvFormat;
        depthDesc.SampleDesc.Count   = m_sampleCount;
        depthDesc.SampleDesc.Quality = 0;
        depthDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depthDesc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format               = m_dsvFormat;
        clearValue.DepthStencil.Depth   = 0.0f;
        clearValue.DepthStencil.Stencil = 0;

        HR_CHECK(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &depthDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearValue,
            IID_PPV_ARGS(&m_depthStencil)
        ));

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format        = m_dsvFormat;
        dsvDesc.ViewDimension = (m_sampleCount > 1) ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags         = D3D12_DSV_FLAG_NONE;

        device->CreateDepthStencilView(m_depthStencil.Get(), &dsvDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    }

    void FrameBuffer::UpdateViewportAndScissor(uint32_t width, uint32_t height)
    {
        m_viewportRect = Maths::Rect(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
        m_viewport     = m_viewportRect.ToD3D12Viewport(0.0f, 1.0f);
        m_scissorRect  = m_viewportRect.ToD3D12Rect();
    }

    void FrameBuffer::CheckMsaaSupport(ID3D12Device* device)
    {
        if (m_sampleCount <= 1)
        {
            m_sampleCount = 1;
            m_msaaQualityLevels = 0;
            return;
        }

        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS rtvQualityLevels = {};
        rtvQualityLevels.Format      = m_rtvFormat;
        rtvQualityLevels.SampleCount = m_sampleCount;
        rtvQualityLevels.Flags       = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS dsvQualityLevels = {};
        dsvQualityLevels.Format      = m_dsvFormat;
        dsvQualityLevels.SampleCount = m_sampleCount;
        dsvQualityLevels.Flags       = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

        const bool rtvSupported = SUCCEEDED(device->CheckFeatureSupport(
            D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
            &rtvQualityLevels,
            sizeof(rtvQualityLevels))) && rtvQualityLevels.NumQualityLevels > 0;

        const bool dsvSupported = SUCCEEDED(device->CheckFeatureSupport(
            D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
            &dsvQualityLevels,
            sizeof(dsvQualityLevels))) && dsvQualityLevels.NumQualityLevels > 0;

        if (rtvSupported && dsvSupported)
        {
            m_msaaQualityLevels = std::min(rtvQualityLevels.NumQualityLevels, dsvQualityLevels.NumQualityLevels);
            std::wcout << L"[FrameBuffer] " << m_sampleCount << L"x MSAA active (Hardware Quality Levels: " << m_msaaQualityLevels << L").\n";
        }
        else
        {
            std::wcout << L"[FrameBuffer] " << m_sampleCount << L"x MSAA not supported on this adapter; falling back to 1x (no MSAA).\n";
            m_sampleCount = 1;
            m_msaaQualityLevels = 0;
        }
    }
}

