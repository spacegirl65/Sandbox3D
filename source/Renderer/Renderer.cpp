// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Renderer.h"

#include <iostream>
#include <filesystem>

namespace Sandbox3D::Renderer
{
    // Embedded fallback HLSL source ensuring zero external file path launch dependencies
    static constexpr const char* s_embeddedVertexShader = R"(
        cbuffer ModelViewProjectionBuffer : register(b0)
        {
            float4x4 g_mvp;
        };

        struct VertexInput
        {
            float3 position : POSITION;
            float3 color    : COLOR;
        };

        struct VertexOutput
        {
            float4 position : SV_POSITION;
            float3 color    : COLOR;
        };

        VertexOutput VSMain(VertexInput input)
        {
            VertexOutput output;
            output.position = mul(float4(input.position, 1.0f), g_mvp);
            output.color = input.color;
            return output;
        }
    )";

    static constexpr const char* s_embeddedPixelShader = R"(
        struct PixelInput
        {
            float4 position : SV_POSITION;
            float3 color    : COLOR;
        };

        float4 PSMain(PixelInput input) : SV_TARGET
        {
            return float4(input.color, 1.0f);
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

        // 2. Compile shaders (attempt file on disk, fallback to embedded source)
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

        // 3. Initialise PipelineState (Root Signature + PSO)
        m_pipelineState.Initialise(device, vertexShader, pixelShader, m_swapChain.GetFormat());

        // 4. Initialise MVP ConstantBuffer
        m_mvpConstantBuffer.Initialise(device);

        // 5. Initialise Geometry: Red Triangle (0, 0), (0, 1), (1, 1)
        m_triangleVertexBuffer = VertexBuffer::CreateRedTriangle(device);

        // 6. Configure Camera looking straight at the triangle
        // The triangle is positioned on XY plane spanning [0, 1] on X and [0, 1] on Y.
        // Center of the triangle is at (0.5, 0.5, 0.0).
        // Camera is positioned at (0.5, 0.5, -2.5) looking directly at (0.5, 0.5, 0.0) with Up (0, 1, 0).
        m_camera.SetLookAt(
            Maths::Vec3D(0.5, 0.5, -2.5),
            Maths::Vec3D(0.5, 0.5, 0.0),
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
        m_mvpConstantBuffer.Shutdown();
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

        // Flush GPU before resizing swap chain back buffers
        m_commandContext.Flush(commandQueue);

        m_swapChain.Resize(device, m_width, m_height);
        UpdateViewportAndScissor(m_width, m_height);
    }

    void Renderer::UpdateViewportAndScissor(uint32_t width, uint32_t height)
    {
        m_viewport.TopLeftX = 0.0f;
        m_viewport.TopLeftY = 0.0f;
        m_viewport.Width    = static_cast<float>(width);
        m_viewport.Height   = static_cast<float>(height);
        m_viewport.MinDepth = 0.0f;
        m_viewport.MaxDepth = 1.0f;

        m_scissorRect.left   = 0;
        m_scissorRect.top    = 0;
        m_scissorRect.right  = static_cast<LONG>(width);
        m_scissorRect.bottom = static_cast<LONG>(height);

        m_camera.UpdateAspectRatio(static_cast<float>(width) / static_cast<float>(height));
    }

    void Renderer::Render(ID3D12CommandQueue* commandQueue)
    {
        const UINT frameIndex = m_swapChain.GetCurrentBackBufferIndex();
        m_commandContext.BeginFrame(frameIndex);

        ID3D12GraphicsCommandList* const commandList = m_commandContext.GetCommandList();
        ID3D12Resource* const renderTarget = m_swapChain.GetCurrentRenderTarget();
        const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_swapChain.GetCurrentRtvHandle();

        // 1. Transition back buffer to render target state
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource   = renderTarget;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);

        // 2. Clear render target view to dark slate grey
        constexpr float clearColor[4] = { 0.12f, 0.14f, 0.18f, 1.0f };
        commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

        // 3. Update ModelViewProjection constant buffer using dual-tier camera-relative math (Rules 19 & 20)
        ModelViewProjectionBuffer cbData;
        cbData.mvp = m_camera.CalculateCameraRelativeMVP(Maths::Mat4x4D::Identity());
        m_mvpConstantBuffer.Update(cbData);

        // 4. Set pipeline state & descriptors
        commandList->RSSetViewports(1, &m_viewport);
        commandList->RSSetScissorRects(1, &m_scissorRect);
        commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        commandList->SetGraphicsRootSignature(m_pipelineState.GetRootSignature());
        commandList->SetGraphicsRootConstantBufferView(0, m_mvpConstantBuffer.GetGpuVirtualAddress());
        commandList->SetPipelineState(m_pipelineState.GetPipelineState());

        // 5. Issue draw call for the red triangle
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        const D3D12_VERTEX_BUFFER_VIEW vbView = m_triangleVertexBuffer.GetView();
        commandList->IASetVertexBuffers(0, 1, &vbView);
        commandList->DrawInstanced(m_triangleVertexBuffer.GetVertexCount(), 1, 0, 0);

        // 6. Transition back buffer to present state
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
        commandList->ResourceBarrier(1, &barrier);

        // 7. Execute command list on GPU and present frame
        m_commandContext.Execute(commandQueue, frameIndex);
        m_swapChain.Present(/* vSync = */ true);
    }
}

