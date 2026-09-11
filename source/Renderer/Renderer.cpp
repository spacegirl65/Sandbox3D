// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Renderer.h"

#include <iostream>
#include <filesystem>
#include <algorithm>

namespace Sandbox3D::Renderer
{
    // Embedded fallback HLSL source ensuring zero external file path launch dependencies
    static constexpr const char* s_embeddedVertexShader = R"(
        cbuffer SceneConstantBuffer : register(b0)
        {
            row_major float4x4 g_mvp;
            row_major float4x4 g_world;
            float4 g_lightDirection;
            float4 g_lightColor;
            float4 g_ambientColor;
        };

        struct VertexInput
        {
            float3 position : POSITION;
            float3 normal   : NORMAL;
            float4 color    : COLOR;
        };

        struct VertexOutput
        {
            float4 position    : SV_POSITION;
            float3 worldNormal : NORMAL;
            float4 color       : COLOR;
        };

        VertexOutput VSMain(VertexInput input)
        {
            VertexOutput output;
            output.position = mul(float4(input.position, 1.0f), g_mvp);
            output.worldNormal = normalize(mul(float4(input.normal, 0.0f), g_world).xyz);
            output.color = input.color;
            return output;
        }
    )";

    static constexpr const char* s_embeddedPixelShader = R"(
        cbuffer SceneConstantBuffer : register(b0)
        {
            row_major float4x4 g_mvp;
            row_major float4x4 g_world;
            float4 g_lightDirection;
            float4 g_lightColor;
            float4 g_ambientColor;
        };

        struct PixelInput
        {
            float4 position    : SV_POSITION;
            float3 worldNormal : NORMAL;
            float4 color       : COLOR;
        };

        float4 PSMain(PixelInput input) : SV_TARGET
        {
            float3 N = normalize(input.worldNormal);
            float3 L = normalize(-g_lightDirection.xyz);
            float nDotL = max(dot(N, L), 0.0f);

            float3 diffuse = g_lightColor.rgb * nDotL;
            float3 ambient = g_ambientColor.rgb;
            float3 shadedColor = input.color.rgb * (ambient + diffuse);

            return float4(shadedColor, input.color.a);
        }
    )";

    void Renderer::Initialise(
        IDXGIFactory6* factory,
        ID3D12Device* device,
        ID3D12CommandQueue* commandQueue,
        HWND hwnd,
        uint32_t width,
        uint32_t height
    )
    {
        m_width  = width;
        m_height = height;

        // 1. Initialise SwapChain & CommandContext
        m_swapChain.Initialise(factory, device, commandQueue, hwnd, m_width, m_height);
        m_commandContext.Initialise(device);

        // 2. Query hardware MSAA support and configure off-screen multisampled targets
        CheckMsaaSupport(device);
        CreateMsaaRenderTarget(device, m_width, m_height);
        CreateDepthStencil(device, m_width, m_height);

        // 3. Compile shaders (attempt file on disk, fallback to embedded source)
        Shader vertexShader;
        const std::filesystem::path vsPath = "source/Shaders/VertexShader.hlsl";
        if (std::filesystem::exists(vsPath))
        {
            vertexShader.CompileFromFile(vsPath, "VSMain", ShaderStage::Vertex);
        }
        else
        {
            vertexShader.CompileFromSource(s_embeddedVertexShader, "EmbeddedVertexShader.hlsl", "VSMain", ShaderStage::Vertex);
        }

        Shader pixelShader;
        const std::filesystem::path psPath = "source/Shaders/PixelShader.hlsl";
        if (std::filesystem::exists(psPath))
        {
            pixelShader.CompileFromFile(psPath, "PSMain", ShaderStage::Pixel);
        }
        else
        {
            pixelShader.CompileFromSource(s_embeddedPixelShader, "EmbeddedPixelShader.hlsl", "PSMain", ShaderStage::Pixel);
        }

        // 4. Initialise PipelineState (Root Signature + PSO) with DSV format and MSAA sample count
        m_pipelineState.Initialise(device, vertexShader, pixelShader, m_swapChain.GetFormat(), DXGI_FORMAT_D32_FLOAT, m_sampleCount);

        // 5. Initialise Scene ConstantBuffers and Orientation Gizmo
        constexpr size_t MaxItemsPerFrame = 1024;
        m_sceneConstantBuffer.Initialise(device, MaxItemsPerFrame * SwapChain::BufferCount);
        m_gizmoConstantBuffer.Initialise(device, SwapChain::BufferCount);
        m_gizmoMesh = Mesh::CreateCoordinateAxes(device);

        // 5. Configure Camera looking straight at the middle of the quad at the origin (0, 0, 0)
        // Camera is positioned at (0.0, 0.0, -2.5) looking directly forward at (0.0, 0.0, 0.0) with Up (0, 1, 0).
        m_camera.SetLookAt(
            Maths::Vec3D(0.0, 0.0, -2.5),
            Maths::Vec3D(0.0, 0.0, 0.0),
            Maths::Vec3D(0.0, 1.0, 0.0)
        );

        UpdateViewportAndScissor(m_width, m_height);

        m_isInitialised = true;
        std::wcout << L"[Renderer] Renderer initialised successfully.\n";
    }

    void Renderer::Shutdown(ID3D12CommandQueue* commandQueue) noexcept
    {
        if (!m_isInitialised)
        {
            return;
        }

        m_commandContext.Shutdown(commandQueue);
        m_sceneConstantBuffer.Shutdown();
        m_gizmoConstantBuffer.Shutdown();
        m_gizmoMesh.reset();
        m_msaaRenderTarget.Reset();
        m_msaaRtvHeap.Reset();
        m_depthStencilBuffer.Reset();
        m_dsvHeap.Reset();
        m_isInitialised = false;
    }

    void Renderer::OnResize(
        ID3D12Device* device,
        ID3D12CommandQueue* commandQueue,
        uint32_t width,
        uint32_t height
    )
    {
        if (!m_isInitialised || width == 0 || height == 0)
        {
            return;
        }

        m_width  = width;
        m_height = height;

        // Flush GPU before resizing swap chain back buffers and off-screen MSAA render targets
        m_commandContext.Flush(commandQueue);

        m_swapChain.Resize(device, m_width, m_height);
        CreateMsaaRenderTarget(device, m_width, m_height);
        CreateDepthStencil(device, m_width, m_height);
        UpdateViewportAndScissor(m_width, m_height);
    }

    void Renderer::UpdateViewportAndScissor(uint32_t width, uint32_t height)
    {
        m_viewportRect = Maths::Rect(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
        m_viewport     = m_viewportRect.ToD3D12Viewport(0.0f, 1.0f);
        m_scissorRect  = m_viewportRect.ToD3D12Rect();

        m_camera.UpdateAspectRatio(static_cast<float>(width) / static_cast<float>(height));
    }

    void Renderer::CheckMsaaSupport(ID3D12Device* device)
    {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS rtvQualityLevels = {};
        rtvQualityLevels.Format      = m_swapChain.GetFormat();
        rtvQualityLevels.SampleCount = m_sampleCount;
        rtvQualityLevels.Flags       = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS dsvQualityLevels = {};
        dsvQualityLevels.Format      = DXGI_FORMAT_D32_FLOAT;
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
            std::wcout << L"[Renderer] " << m_sampleCount << L"x MSAA active (Hardware Quality Levels: " << m_msaaQualityLevels << L").\n";
        }
        else
        {
            std::wcout << L"[Renderer] " << m_sampleCount << L"x MSAA not supported on this adapter; falling back to 1x (no MSAA).\n";
            m_sampleCount = 1;
            m_msaaQualityLevels = 0;
        }
    }

    void Renderer::CreateMsaaRenderTarget(ID3D12Device* device, uint32_t width, uint32_t height)
    {
        m_msaaRenderTarget.Reset();

        if (m_sampleCount <= 1)
        {
            return;
        }

        if (!m_msaaRtvHeap)
        {
            D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
            rtvHeapDesc.NumDescriptors = 1;
            rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            HR_CHECK(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_msaaRtvHeap)));
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
        rtvDesc.Format             = m_swapChain.GetFormat();
        rtvDesc.SampleDesc.Count   = m_sampleCount;
        rtvDesc.SampleDesc.Quality = 0;
        rtvDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        rtvDesc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        const float clearColor[4] = { m_clearColor.r(), m_clearColor.g(), m_clearColor.b(), m_clearColor.a() };
        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format   = m_swapChain.GetFormat();
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
            IID_PPV_ARGS(&m_msaaRenderTarget)
        ));

        D3D12_RENDER_TARGET_VIEW_DESC rtvViewDesc = {};
        rtvViewDesc.Format        = m_swapChain.GetFormat();
        rtvViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;

        device->CreateRenderTargetView(m_msaaRenderTarget.Get(), &rtvViewDesc, m_msaaRtvHeap->GetCPUDescriptorHandleForHeapStart());
    }

    void Renderer::CreateDepthStencil(ID3D12Device* device, uint32_t width, uint32_t height)
    {
        m_depthStencilBuffer.Reset();

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
        depthDesc.Format             = DXGI_FORMAT_D32_FLOAT;
        depthDesc.SampleDesc.Count   = m_sampleCount;
        depthDesc.SampleDesc.Quality = 0;
        depthDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depthDesc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format               = DXGI_FORMAT_D32_FLOAT;
        clearValue.DepthStencil.Depth   = 1.0f;
        clearValue.DepthStencil.Stencil = 0;

        HR_CHECK(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &depthDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearValue,
            IID_PPV_ARGS(&m_depthStencilBuffer)
        ));

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format        = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = (m_sampleCount > 1) ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags         = D3D12_DSV_FLAG_NONE;

        device->CreateDepthStencilView(m_depthStencilBuffer.Get(), &dsvDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    }

    void Renderer::Render(ID3D12CommandQueue* commandQueue, std::span<const RenderItem> renderItems)
    {
        const UINT frameIndex = m_swapChain.GetCurrentBackBufferIndex();
        m_commandContext.BeginFrame(frameIndex);

        ID3D12GraphicsCommandList* const commandList = m_commandContext.GetCommandList();
        ID3D12Resource* const backBuffer = m_swapChain.GetCurrentRenderTarget();
        const D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv = m_swapChain.GetCurrentRtvHandle();
        const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

        const bool useMsaa = (m_sampleCount > 1 && m_msaaRenderTarget);
        const D3D12_CPU_DESCRIPTOR_HANDLE activeRtv = useMsaa ? m_msaaRtvHeap->GetCPUDescriptorHandleForHeapStart() : backBufferRtv;

        // 1. If not using MSAA, transition back buffer to render target state
        if (!useMsaa)
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource   = backBuffer;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            commandList->ResourceBarrier(1, &barrier);
        }

        // 2. Clear render target view to dark slate grey using Vec4, and clear depth stencil view
        const float clearColor[4] = { m_clearColor.r(), m_clearColor.g(), m_clearColor.b(), m_clearColor.a() };
        commandList->ClearRenderTargetView(activeRtv, clearColor, 0, nullptr);
        commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        // 3. Set pipeline state & descriptors
        commandList->RSSetViewports(1, &m_viewport);
        commandList->RSSetScissorRects(1, &m_scissorRect);
        commandList->OMSetRenderTargets(1, &activeRtv, FALSE, &dsvHandle);

        commandList->SetGraphicsRootSignature(m_pipelineState.GetRootSignature());
        commandList->SetPipelineState(m_pipelineState.GetPipelineState());

        // 4. Iterate over active render items, updating camera-relative MVP per object and issuing draw calls
        constexpr size_t MaxItemsPerFrame = 1024;
        size_t itemIndex = 0;

        for (const auto& item : renderItems)
        {
            if (!item.isVisible || !item.mesh)
            {
                continue;
            }

            if (itemIndex >= MaxItemsPerFrame)
            {
                break;
            }

            const size_t slotIndex = frameIndex * MaxItemsPerFrame + itemIndex;

            // Update SceneConstantBuffer with camera-relative MVP, world matrix, and directional lighting parameters
            SceneConstantBuffer cbData;
            cbData.mvp            = m_camera.CalculateCameraRelativeMVP(item.worldMatrix);
            cbData.world          = Maths::Mat4x4(item.worldMatrix);
            cbData.lightDirection = m_lightDirection;
            cbData.lightColor     = m_lightColor;
            cbData.ambientColor   = m_ambientColor;
            m_sceneConstantBuffer.Update(cbData, slotIndex);

            commandList->SetGraphicsRootConstantBufferView(0, m_sceneConstantBuffer.GetGpuVirtualAddress(slotIndex));
            item.mesh->Draw(commandList);

            ++itemIndex;
        }

        // 5. Render World-Space Orientation Gizmo in the top-left corner
        if (m_showGizmo && m_gizmoMesh && m_gizmoMesh->IsInitialised())
        {
            const float margin = m_gizmoMargin;
            const float size   = m_gizmoSize;

            D3D12_VIEWPORT gizmoViewport{};
            gizmoViewport.TopLeftX = margin;
            gizmoViewport.TopLeftY = margin;
            gizmoViewport.Width    = size;
            gizmoViewport.Height   = size;
            gizmoViewport.MinDepth = 0.0f;
            gizmoViewport.MaxDepth = 1.0f;

            D3D12_RECT gizmoScissor{};
            gizmoScissor.left   = static_cast<LONG>(margin);
            gizmoScissor.top    = static_cast<LONG>(margin);
            gizmoScissor.right  = static_cast<LONG>(margin + size);
            gizmoScissor.bottom = static_cast<LONG>(margin + size);

            commandList->RSSetViewports(1, &gizmoViewport);
            commandList->RSSetScissorRects(1, &gizmoScissor);

            // Clear depth within gizmo region so it renders on top of scene geometry while depth-testing against itself
            commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 1, &gizmoScissor);

            // Extract camera's view rotation matrix and offset along view Z
            const auto rot = m_camera.GetViewMatrix().GetRotationMatrix();
            Maths::Mat4x4 gizmoView(
                static_cast<float>(rot.m[0][0]), static_cast<float>(rot.m[0][1]), static_cast<float>(rot.m[0][2]), 0.0f,
                static_cast<float>(rot.m[1][0]), static_cast<float>(rot.m[1][1]), static_cast<float>(rot.m[1][2]), 0.0f,
                static_cast<float>(rot.m[2][0]), static_cast<float>(rot.m[2][1]), static_cast<float>(rot.m[2][2]), 0.0f,
                0.0f,                            0.0f,                            3.0f,                            1.0f
            );

            // Square orthographic projection to prevent perspective distortion at corner
            const Maths::Mat4x4 gizmoProj = Maths::Mat4x4::Orthographic(2.8f, 2.8f, 0.1f, 10.0f);

            SceneConstantBuffer gizmoCb;
            gizmoCb.mvp            = gizmoView * gizmoProj;
            gizmoCb.world          = Maths::Mat4x4::Identity();
            // Unlit shading: zero directional diffuse contribution and unit ambient multiplier
            gizmoCb.lightDirection = Maths::Vec4::Zero();
            gizmoCb.lightColor     = Maths::Vec4::Zero();
            gizmoCb.ambientColor   = Maths::Vec4::One();
            m_gizmoConstantBuffer.Update(gizmoCb, frameIndex);

            commandList->SetGraphicsRootConstantBufferView(0, m_gizmoConstantBuffer.GetGpuVirtualAddress(frameIndex));
            m_gizmoMesh->Draw(commandList);

            // Restore primary viewport and scissor rect
            commandList->RSSetViewports(1, &m_viewport);
            commandList->RSSetScissorRects(1, &m_scissorRect);
        }

        // 6. Transition and resolve to swap chain back buffer
        if (useMsaa)
        {
            // Transition MSAA render target from RENDER_TARGET to RESOLVE_SOURCE
            // Transition swap chain back buffer from PRESENT to RESOLVE_DEST
            D3D12_RESOURCE_BARRIER preResolveBarriers[2] = {};
            preResolveBarriers[0].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            preResolveBarriers[0].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            preResolveBarriers[0].Transition.pResource   = m_msaaRenderTarget.Get();
            preResolveBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            preResolveBarriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
            preResolveBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            preResolveBarriers[1].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            preResolveBarriers[1].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            preResolveBarriers[1].Transition.pResource   = backBuffer;
            preResolveBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            preResolveBarriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_RESOLVE_DEST;
            preResolveBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            commandList->ResourceBarrier(2, preResolveBarriers);

            // Hardware resolve multisampled texture to swap chain back buffer
            commandList->ResolveSubresource(
                backBuffer, 0,
                m_msaaRenderTarget.Get(), 0,
                m_swapChain.GetFormat()
            );

            // Transition swap chain back buffer from RESOLVE_DEST to PRESENT
            // Transition MSAA render target from RESOLVE_SOURCE back to RENDER_TARGET for next frame
            D3D12_RESOURCE_BARRIER postResolveBarriers[2] = {};
            postResolveBarriers[0].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            postResolveBarriers[0].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            postResolveBarriers[0].Transition.pResource   = backBuffer;
            postResolveBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RESOLVE_DEST;
            postResolveBarriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
            postResolveBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            postResolveBarriers[1].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            postResolveBarriers[1].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            postResolveBarriers[1].Transition.pResource   = m_msaaRenderTarget.Get();
            postResolveBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
            postResolveBarriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
            postResolveBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            commandList->ResourceBarrier(2, postResolveBarriers);
        }
        else
        {
            // Transition back buffer from RENDER_TARGET to PRESENT
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource   = backBuffer;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            commandList->ResourceBarrier(1, &barrier);
        }

        // 7. Execute command list on GPU and present frame
        m_commandContext.Execute(commandQueue, frameIndex);
        m_swapChain.Present(/* vSync = */ true);
    }
}

