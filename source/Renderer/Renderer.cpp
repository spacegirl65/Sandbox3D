// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Renderer.h"

#include <iostream>
#include <filesystem>

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

        // 4. Initialise Scene ConstantBuffer
        m_sceneConstantBuffer.Initialise(device);

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
        m_viewportRect = Maths::Rect(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
        m_viewport     = m_viewportRect.ToD3D12Viewport(0.0f, 1.0f);
        m_scissorRect  = m_viewportRect.ToD3D12Rect();

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

        // 2. Clear render target view to dark slate grey using Vec4
        const float clearColor[4] = { m_clearColor.r(), m_clearColor.g(), m_clearColor.b(), m_clearColor.a() };
        commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

        // 3. Set pipeline state & descriptors
        commandList->RSSetViewports(1, &m_viewport);
        commandList->RSSetScissorRects(1, &m_scissorRect);
        commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        commandList->SetGraphicsRootSignature(m_pipelineState.GetRootSignature());
        commandList->SetPipelineState(m_pipelineState.GetPipelineState());

        // 4. Iterate over active render items, updating camera-relative MVP per object and issuing draw calls
        for (const auto& item : m_renderItems)
        {
            if (!item.isVisible || !item.mesh)
            {
                continue;
            }

            // Update SceneConstantBuffer with camera-relative MVP, world matrix, and directional lighting parameters
            SceneConstantBuffer cbData;
            cbData.mvp            = m_camera.CalculateCameraRelativeMVP(item.worldMatrix);
            cbData.world          = Maths::Mat4x4(item.worldMatrix);
            cbData.lightDirection = m_lightDirection;
            cbData.lightColor     = m_lightColor;
            cbData.ambientColor   = m_ambientColor;
            m_sceneConstantBuffer.Update(cbData);

            commandList->SetGraphicsRootConstantBufferView(0, m_sceneConstantBuffer.GetGpuVirtualAddress());
            item.mesh->Draw(commandList);
        }

        // 5. Transition back buffer to present state
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
        commandList->ResourceBarrier(1, &barrier);

        // 7. Execute command list on GPU and present frame
        m_commandContext.Execute(commandQueue, frameIndex);
        m_swapChain.Present(/* vSync = */ true);
    }
}

