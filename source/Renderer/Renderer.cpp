// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Renderer.h"
#include "EmbeddedShaders.h"

#include <iostream>
#include <filesystem>
#include <algorithm>

namespace Sandbox3D::Renderer
{
    void Renderer::Initialise(
        IDXGIFactory6* factory,
        ID3D12Device* device,
        ID3D12CommandQueue* commandQueue,
        HWND hwnd,
        uint32_t width,
        uint32_t height,
        const std::wstring& gpuDescription
    )
    {
        m_width  = width;
        m_height = height;

        if (!gpuDescription.empty())
        {
            const int sizeNeeded = WideCharToMultiByte(
                CP_UTF8,
                0,
                gpuDescription.data(),
                static_cast<int>(gpuDescription.size()),
                nullptr,
                0,
                nullptr,
                nullptr
            );
            if (sizeNeeded > 0)
            {
                m_gpuName.resize(sizeNeeded);
                WideCharToMultiByte(
                    CP_UTF8,
                    0,
                    gpuDescription.data(),
                    static_cast<int>(gpuDescription.size()),
                    m_gpuName.data(),
                    sizeNeeded,
                    nullptr,
                    nullptr
                );
            }
        }

        // Initialise SwapChain & CommandContext
        m_swapChain.Initialise(factory, device, commandQueue, hwnd, m_width, m_height);
        m_commandContext.Initialise(device);

        // Initialise off-screen multisampled frame buffer
        m_frameBuffer.Initialise(
            device,
            m_width,
            m_height,
            m_swapChain.GetFormat(),
            DXGI_FORMAT_D32_FLOAT,
            4,
            m_clearColor
        );

        // Initialise combined shader pipelines (1 file per material containing VSMain and PSMain)
        // Terrain shader (source/Shaders/Terrain.hlsl)
        Shader terrainVs;
        Shader terrainPs;
        const std::filesystem::path terrainPath = "source/Shaders/Terrain.hlsl";
        if (std::filesystem::exists(terrainPath))
        {
            terrainVs.CompileFromFile(terrainPath, "VSMain", ShaderStage::Vertex);
            terrainPs.CompileFromFile(terrainPath, "PSMain", ShaderStage::Pixel);
        }
        else
        {
            const std::string terrainSource = std::string(s_embeddedSceneBuffers) + s_embeddedVertexShaderStage + s_embeddedTerrainPixelStage;
            terrainVs.CompileFromSource(terrainSource, "EmbeddedTerrain.hlsl", "VSMain", ShaderStage::Vertex);
            terrainPs.CompileFromSource(terrainSource, "EmbeddedTerrain.hlsl", "PSMain", ShaderStage::Pixel);
        }

        // Initialise primary root signature, Terrain PipelineState, and Depth-Only Pre-Pass PipelineState
        m_pipelineState.Initialise(
            device,
            terrainVs,
            terrainPs,
            m_swapChain.GetFormat(),
            DXGI_FORMAT_D32_FLOAT,
            m_frameBuffer.GetSampleCount(),
            0,
            /* depthWrite = */ true,
            D3D12_COMPARISON_FUNC_GREATER_EQUAL
        );

        m_depthPipelineState.InitialiseDepthOnly(
            device,
            m_pipelineState.GetRootSignature(),
            terrainVs,
            DXGI_FORMAT_D32_FLOAT,
            m_frameBuffer.GetSampleCount(),
            0,
            D3D12_COMPARISON_FUNC_GREATER_EQUAL
        );

        // Standard mesh shader (source/Shaders/Standard.hlsl)
        Shader standardVs;
        Shader standardPs;
        const std::filesystem::path standardPath = "source/Shaders/Standard.hlsl";
        if (std::filesystem::exists(standardPath))
        {
            standardVs.CompileFromFile(standardPath, "VSMain", ShaderStage::Vertex);
            standardPs.CompileFromFile(standardPath, "PSMain", ShaderStage::Pixel);
        }
        else
        {
            const std::string standardSource = std::string(s_embeddedSceneBuffers) + s_embeddedVertexShaderStage + s_embeddedStandardPixelStage;
            standardVs.CompileFromSource(standardSource, "EmbeddedStandard.hlsl", "VSMain", ShaderStage::Vertex);
            standardPs.CompileFromSource(standardSource, "EmbeddedStandard.hlsl", "PSMain", ShaderStage::Pixel);
        }

        PipelineState standardPso;
        standardPso.Initialise(
            device,
            m_pipelineState.GetRootSignature(),
            standardVs,
            standardPs,
            m_swapChain.GetFormat(),
            DXGI_FORMAT_D32_FLOAT,
            m_frameBuffer.GetSampleCount(),
            0,
            /* depthWrite = */ true,
            D3D12_COMPARISON_FUNC_GREATER_EQUAL
        );
        m_pipelineStates.emplace("Standard", std::move(standardPso));

        // Unlit mesh shader (source/Shaders/Unlit.hlsl)
        Shader unlitVs;
        Shader unlitPs;
        const std::filesystem::path unlitPath = "source/Shaders/Unlit.hlsl";
        if (std::filesystem::exists(unlitPath))
        {
            unlitVs.CompileFromFile(unlitPath, "VSMain", ShaderStage::Vertex);
            unlitPs.CompileFromFile(unlitPath, "PSMain", ShaderStage::Pixel);
        }
        else
        {
            const std::string unlitSource = std::string(s_embeddedSceneBuffers) + s_embeddedVertexShaderStage + s_embeddedUnlitPixelStage;
            unlitVs.CompileFromSource(unlitSource, "EmbeddedUnlit.hlsl", "VSMain", ShaderStage::Vertex);
            unlitPs.CompileFromSource(unlitSource, "EmbeddedUnlit.hlsl", "PSMain", ShaderStage::Pixel);
        }

        m_unlitPipelineState.Initialise(
            device,
            m_pipelineState.GetRootSignature(),
            unlitVs,
            unlitPs,
            m_swapChain.GetFormat(),
            DXGI_FORMAT_D32_FLOAT,
            m_frameBuffer.GetSampleCount(),
            0,
            /* depthWrite = */ true,
            D3D12_COMPARISON_FUNC_GREATER_EQUAL
        );

        // Sky dome / celestial raymarching shader (source/Shaders/Sky.hlsl)
        Shader skyVs;
        Shader skyPs;
        const std::filesystem::path skyPath = "source/Shaders/Sky.hlsl";
        if (std::filesystem::exists(skyPath))
        {
            skyVs.CompileFromFile(skyPath, "VSMain", ShaderStage::Vertex);
            skyPs.CompileFromFile(skyPath, "PSMain", ShaderStage::Pixel);
        }

        if (skyVs.GetBufferSize() > 0 && skyPs.GetBufferSize() > 0)
        {
            // Reverse-Z sky pass: tests for depth >= 0.0 (un-occluded background pixels) with zero depth writing
            m_skyPipelineState.Initialise(
                device,
                m_pipelineState.GetRootSignature(),
                skyVs,
                skyPs,
                m_swapChain.GetFormat(),
                DXGI_FORMAT_D32_FLOAT,
                m_frameBuffer.GetSampleCount(),
                0,
                /* depthWrite = */ false,
                D3D12_COMPARISON_FUNC_GREATER_EQUAL,
                D3D12_CULL_MODE_NONE
            );
        }

        // Initialise Dynamic Constant Buffer Ring Allocator, Orientation Gizmo, and Diagnostic Text Overlay
        m_dynamicConstantBuffer.Initialise(device, DynamicUploadBuffer::DefaultPageSize, SwapChain::BufferCount);
        m_gizmoMesh = Mesh::CreateCoordinateAxes(device);

        m_textOverlay = std::make_unique<TextOverlay>();
        m_textOverlay->Initialise(device);

        // Initialise Cascaded Directional Shadows (CSM) resources and pipeline state
        m_cascadedShadowMap.Initialise(device, m_pipelineState.GetRootSignature());

        // Initialise terrain PBR textures and GPU descriptor tables
        InitialiseTextureResources(device, commandQueue);

        m_lastFrameTime = std::chrono::high_resolution_clock::now();
        m_smoothedFps   = 120.0f;
        m_smoothedFrameTimeMs = 8.33f;
        m_fpsTimeAccumulator  = 0.0f;
        m_fpsFrameCount       = 0;

        m_camera->UpdateAspectRatio(static_cast<float>(m_width) / static_cast<float>(m_height));

        m_isInitialised = true;
        std::wcout << L"[Renderer] Renderer initialised successfully.\n";
    }

    void Renderer::Shutdown(ID3D12CommandQueue* commandQueue) noexcept
    {
        if (!m_isInitialised)
        {
            return;
        }

        if (m_textOverlay)
        {
            m_textOverlay->Shutdown();
            m_textOverlay.reset();
        }

        m_cascadedShadowMap.Shutdown();
        m_terrainTextures.clear();
        m_lidarOcclusionTexture.reset();
        m_lidarHorizonTexture0.reset();
        m_lidarHorizonTexture1.reset();
        m_srvHeap.Reset();
        m_commandContext.Shutdown(commandQueue);
        m_dynamicConstantBuffer.Shutdown();
        m_gizmoMesh.reset();
        m_frameBuffer.Shutdown();
        m_depthPipelineState = {};
        m_skyPipelineState = {};
        m_pipelineStates.clear();
        m_isInitialised = false;
    }

    PipelineState* Renderer::GetPipelineState(const std::string& shaderName) noexcept
    {
        if (shaderName == "Terrain")
        {
            return &m_pipelineState;
        }
        if (shaderName == "Unlit")
        {
            return &m_unlitPipelineState;
        }

        auto it = m_pipelineStates.find(shaderName);
        if (it != m_pipelineStates.end())
        {
            return &it->second;
        }
        auto standardIt = m_pipelineStates.find("Standard");
        if (standardIt != m_pipelineStates.end())
        {
            return &standardIt->second;
        }
        return &m_pipelineState;
    }

    const PipelineState* Renderer::GetPipelineState(const std::string& shaderName) const noexcept
    {
        if (shaderName == "Terrain")
        {
            return &m_pipelineState;
        }
        if (shaderName == "Unlit")
        {
            return &m_unlitPipelineState;
        }

        auto it = m_pipelineStates.find(shaderName);
        if (it != m_pipelineStates.end())
        {
            return &it->second;
        }
        auto standardIt = m_pipelineStates.find("Standard");
        if (standardIt != m_pipelineStates.end())
        {
            return &standardIt->second;
        }
        return &m_pipelineState;
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

        // Flush GPU before resizing swap chain back buffers and off-screen frame buffer
        m_commandContext.Flush(commandQueue);

        m_swapChain.Resize(device, m_width, m_height);
        m_frameBuffer.Resize(device, m_width, m_height);
        m_camera->UpdateAspectRatio(static_cast<float>(width) / static_cast<float>(height));
    }

    void Renderer::BeginFrame(
        ID3D12GraphicsCommandList* commandList,
        std::span<const GpuLight> lights,
        SceneConstantBuffer& commonCbData
    )
    {
        if (!m_enableCascadedShadows || !m_cascadedShadowMap.IsInitialised() || !m_camera)
        {
            return;
        }

        // Identify primary directional solar illumination vector
        Maths::Vec3 sunDir(m_defaultLight.direction.x, m_defaultLight.direction.y, m_defaultLight.direction.z);
        for (uint32_t i = 0; i < commonCbData.lightCount; ++i)
        {
            const uint32_t packed = *reinterpret_cast<const uint32_t*>(&commonCbData.lights[i].direction.w);
            const uint32_t lightType = packed & 0xFu;
            if (lightType == 0u) // Directional light
            {
                sunDir = Maths::Vec3(
                    commonCbData.lights[i].direction.x,
                    commonCbData.lights[i].direction.y,
                    commonCbData.lights[i].direction.z
                );
                break;
            }
        }

        // Partition spectator view frustum into logarithmic depth splits and snap projection boundaries to discrete shadow texels
        m_cascadedShadowMap.UpdateCascades(*m_camera, sunDir);

        // Clear cascade depth slices and render terrain and active entities using light view-projection matrices
        m_cascadedShadowMap.ExecuteShadowPass(
            commandList,
            m_renderQueue.GetOpaqueBatches(),
            m_camera,
            m_dynamicConstantBuffer,
            commonCbData
        );

        // Restore primary framebuffer viewport and scissor rect
        commandList->RSSetViewports(1, &m_frameBuffer.GetViewport());
        commandList->RSSetScissorRects(1, &m_frameBuffer.GetScissorRect());

        // Populate common scene constant buffer with cascade transformation matrices and split depths
        for (uint32_t c = 0; c < CascadedShadowMap::CascadeCount; ++c)
        {
            commonCbData.shadowViewProj[c] = m_cascadedShadowMap.GetLightViewProj(c);
        }
        commonCbData.cascadeSplits = m_cascadedShadowMap.GetCascadeSplits();
        commonCbData.shadowParams  = m_cascadedShadowMap.GetShadowParams();
    }

    void Renderer::Render(
        ID3D12CommandQueue* commandQueue,
        std::span<const RenderItem> renderItems,
        std::span<const GpuLight> lights,
        bool vSync,
        size_t totalSceneItems,
        float updatesPerSecond,
        float targetUps
    )
    {
        const UINT frameIndex = m_swapChain.GetCurrentBackBufferIndex();
        m_commandContext.BeginFrame(frameIndex);
        m_dynamicConstantBuffer.BeginFrame(frameIndex);

        ID3D12GraphicsCommandList* const commandList = m_commandContext.GetCommandList();
        ID3D12Resource* const backBuffer = m_swapChain.GetCurrentRenderTarget();

        // Set root signature
        commandList->SetGraphicsRootSignature(m_pipelineState.GetRootSignature());

        // Bind texture descriptor heap and table across all passes
        if (m_srvHeap)
        {
            ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
            commandList->SetDescriptorHeaps(1, heaps);
            commandList->SetGraphicsRootDescriptorTable(2, m_srvHeap->GetGPUDescriptorHandleForHeapStart());
            commandList->SetGraphicsRoot32BitConstant(3, m_enableAlternativeTextures ? 1u : 0u, 0);
        }

        const Maths::Vec3D cameraPosition = m_camera ? m_camera->GetPosition() : Maths::Vec3D::Zero();
        const Maths::BoundingFrustumD cullingFrustum = m_camera ? m_camera->GetFrustumD() : Maths::BoundingFrustumD();
        const Maths::BoundingFrustumD* pCullingFrustum = m_camera ? &cullingFrustum : nullptr;

        // Build, cull against active view frustum, and sort the render queue into state-minimised batches
        m_renderQueue.Build(renderItems, cameraPosition, m_pipelineStates, m_pipelineState, pCullingFrustum);

        // Pre-populate per-frame common scene lighting and atmospheric parameters
        SceneConstantBuffer commonCbData{};
        commonCbData.ambientColor     = m_ambientColor;
        commonCbData.fogColor         = m_fogColor;
        commonCbData.fogParams        = m_fogParams;
        commonCbData.fogParams.w      = static_cast<float>(cameraPosition.y);
        commonCbData.rayleighParams   = m_rayleighParams;
        commonCbData.mieParams        = m_mieParams;
        commonCbData.ozoneParams      = m_ozoneParams;
        commonCbData.atmosphereParams = m_atmosphereParams;

        if (!lights.empty())
        {
            const uint32_t count = std::min(static_cast<uint32_t>(lights.size()), MaxLights);
            commonCbData.lightCount = count;
            for (uint32_t i = 0; i < count; ++i)
            {
                commonCbData.lights[i] = lights[i];
            }
        }
        else
        {
            commonCbData.lightCount = 1;
            commonCbData.lights[0]  = m_defaultLight;
        }

        // -------------------------------------------------------------
        // Cascaded Directional Shadows (CSM) Pass
        // -------------------------------------------------------------
        BeginFrame(commandList, lights, commonCbData);

        // -------------------------------------------------------------
        // Pass 1: Depth Pre-Pass (Early-Z population, zero pixel shading)
        // -------------------------------------------------------------
        if (m_enableDepthPrePass && m_depthPipelineState.GetPipelineState())
        {
            // Bind depth-stencil buffer only (0 colour render targets) and clear depth
            m_frameBuffer.BindDepthOnly(commandList);
            m_frameBuffer.ClearDepth(commandList, 0.0f, 0);
            commandList->SetPipelineState(m_depthPipelineState.GetPipelineState());

            const Mesh* depthBoundMesh = nullptr;
            for (const auto& batch : m_renderQueue.GetOpaqueBatches())
            {
                if (batch.items.empty() || !batch.mesh)
                {
                    continue;
                }

                // Bypass continuous heightfield terrain from the depth pre-pass, as hardware Early-Z evaluates depth directly in the primary forward shading pass.
                if (batch.material && batch.material->IsTerrain())
                {
                    continue;
                }

                if (batch.mesh != depthBoundMesh)
                {
                    depthBoundMesh = batch.mesh;
                    depthBoundMesh->Bind(commandList);
                }

                const size_t instanceCount = batch.items.size();
                const bool canUseDirectPath = (instanceCount == 1 && batch.items[0]->colorTint == Maths::Vec4::One());

                if (canUseDirectPath)
                {
                    const auto* item = batch.items[0];
                    SceneConstantBuffer cbData = commonCbData;
                    cbData.mvp         = m_camera->CalculateCameraRelativeMVP(item->worldMatrix);
                    cbData.world       = m_camera ? m_camera->CalculateCameraRelativeWorld(item->worldMatrix) : Maths::Mat4x4(item->worldMatrix);
                    cbData.isInstanced = 0;

                    const DynamicAllocation cbAlloc = m_dynamicConstantBuffer.Allocate(cbData);
                    commandList->SetGraphicsRootConstantBufferView(0, cbAlloc.gpuAddress);

                    depthBoundMesh->DrawBound(commandList, 1, 0);
                }
                else
                {
                    SceneConstantBuffer cbData = commonCbData;
                    cbData.mvp         = Maths::Mat4x4::Identity();
                    cbData.world       = Maths::Mat4x4::Identity();
                    cbData.isInstanced = 1;

                    const DynamicAllocation cbAlloc = m_dynamicConstantBuffer.Allocate(cbData);
                    commandList->SetGraphicsRootConstantBufferView(0, cbAlloc.gpuAddress);

                    const size_t byteSize = instanceCount * sizeof(GpuInstanceData);
                    const DynamicAllocation instAlloc = m_dynamicConstantBuffer.Allocate(byteSize, 16);
                    auto* dest = static_cast<GpuInstanceData*>(instAlloc.cpuAddress);

                    for (size_t i = 0; i < instanceCount; ++i)
                    {
                        const auto* item = batch.items[i];
                        dest[i].cameraRelativeMVP   = m_camera->CalculateCameraRelativeMVP(item->worldMatrix);
                        dest[i].cameraRelativeWorld = m_camera ? m_camera->CalculateCameraRelativeWorld(item->worldMatrix) : Maths::Mat4x4(item->worldMatrix);
                        dest[i].colorTint           = item->colorTint;
                    }

                    commandList->SetGraphicsRootShaderResourceView(1, instAlloc.gpuAddress);
                    depthBoundMesh->DrawBound(commandList, static_cast<uint32_t>(instanceCount), 0);
                }
            }
        }

        // -------------------------------------------------------------
        // Pass 2: Main Scene Forward Shading Pass (with Early-Z depth test)
        // -------------------------------------------------------------
        m_frameBuffer.Bind(commandList);
        m_frameBuffer.ClearRenderTarget(commandList);
        if (!m_enableDepthPrePass || !m_depthPipelineState.GetPipelineState())
        {
            m_frameBuffer.ClearDepth(commandList, 0.0f, 0);
        }

        ID3D12PipelineState* currentPso = nullptr;
        const Mesh* currentBoundMesh    = nullptr;

        auto executeBatches = [&](std::span<const RenderBatch> batches)
        {
            for (const auto& batch : batches)
            {
                if (batch.items.empty() || !batch.mesh)
                {
                    continue;
                }

                // Pipeline state transition: update only when pipeline state changes
                ID3D12PipelineState* const targetPso = (batch.pso && batch.pso->GetPipelineState())
                    ? batch.pso->GetPipelineState()
                    : m_pipelineState.GetPipelineState();

                if (currentPso != targetPso && targetPso)
                {
                    currentPso = targetPso;
                    commandList->SetPipelineState(currentPso);
                }

                // Input Assembler binding: bind buffers only when mesh geometry changes
                if (batch.mesh != currentBoundMesh)
                {
                    currentBoundMesh = batch.mesh;
                    currentBoundMesh->Bind(commandList);
                }

                const size_t instanceCount = batch.items.size();
                const bool canUseDirectPath = (instanceCount == 1 && batch.items[0]->colorTint == Maths::Vec4::One());

                if (canUseDirectPath)
                {
                    // Single untinted instance direct path (e.g. landscape terrain mesh)
                    const auto* item = batch.items[0];
                    SceneConstantBuffer cbData = commonCbData;
                    cbData.mvp                 = m_camera->CalculateCameraRelativeMVP(item->worldMatrix);
                    cbData.world               = m_camera ? m_camera->CalculateCameraRelativeWorld(item->worldMatrix) : Maths::Mat4x4(item->worldMatrix);
                    cbData.isInstanced         = 0;
                    cbData.objectLightChannels = item->lightChannels;

                    const DynamicAllocation cbAlloc = m_dynamicConstantBuffer.Allocate(cbData);
                    commandList->SetGraphicsRootConstantBufferView(0, cbAlloc.gpuAddress);

                    currentBoundMesh->DrawBound(commandList, 1, 0);
                }
                else
                {
                    // Hardware instanced multi-item dispatch: 1 draw call across all N instances
                    SceneConstantBuffer cbData = commonCbData;
                    cbData.mvp                 = Maths::Mat4x4::Identity();
                    cbData.world               = Maths::Mat4x4::Identity();
                    cbData.isInstanced         = 1;
                    cbData.objectLightChannels = batch.items[0]->lightChannels;

                    const DynamicAllocation cbAlloc = m_dynamicConstantBuffer.Allocate(cbData);
                    commandList->SetGraphicsRootConstantBufferView(0, cbAlloc.gpuAddress);

                    const size_t byteSize = instanceCount * sizeof(GpuInstanceData);
                    const DynamicAllocation instAlloc = m_dynamicConstantBuffer.Allocate(byteSize, 16);
                    auto* dest = static_cast<GpuInstanceData*>(instAlloc.cpuAddress);

                    for (size_t i = 0; i < instanceCount; ++i)
                    {
                        const auto* item = batch.items[i];
                        dest[i].cameraRelativeMVP   = m_camera->CalculateCameraRelativeMVP(item->worldMatrix);
                        dest[i].cameraRelativeWorld = m_camera ? m_camera->CalculateCameraRelativeWorld(item->worldMatrix) : Maths::Mat4x4(item->worldMatrix);
                        dest[i].colorTint           = item->colorTint;
                    }

                    commandList->SetGraphicsRootShaderResourceView(1, instAlloc.gpuAddress);
                    currentBoundMesh->DrawBound(commandList, static_cast<uint32_t>(instanceCount), 0);
                }
            }
        };

        // Execute opaque batches, followed by transparent batches
        executeBatches(m_renderQueue.GetOpaqueBatches());
        executeBatches(m_renderQueue.GetTransparentBatches());

        // Reverse-Z celestial sky dome and solar disc pass
        if (m_skyPipelineState.GetPipelineState() && m_atmosphereParams.x > 0.0f)
        {
            commandList->SetPipelineState(m_skyPipelineState.GetPipelineState());

            // Prepare scene constant buffer for full-screen unprojection
            SceneConstantBuffer skyCb = commonCbData;
            skyCb.mvp         = m_camera ? m_camera->GetProjectionMatrix() : Maths::Mat4x4::Identity();
            skyCb.world       = m_camera ? Maths::Mat4x4(m_camera->GetViewMatrix().Inverted()) : Maths::Mat4x4::Identity();
            skyCb.isInstanced = 0;

            const DynamicAllocation skyAlloc = m_dynamicConstantBuffer.Allocate(skyCb);
            commandList->SetGraphicsRootConstantBufferView(0, skyAlloc.gpuAddress);

            // Draw full-screen triangle covering the screen at Reverse-Z depth 0.0 (infinite distance)
            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            commandList->DrawInstanced(3, 1, 0, 0);
        }

        if (currentPso != m_pipelineState.GetPipelineState())
        {
            commandList->SetPipelineState(m_pipelineState.GetPipelineState());
        }

        // Render World-Space Orientation Gizmo in the top-left corner
        if (m_showGizmo && m_gizmoMesh && m_gizmoMesh->IsInitialised())
        {
            const float marginX = m_gizmoMarginX;
            const float marginY = m_gizmoMarginY;
            const float size    = m_gizmoSize;

            D3D12_VIEWPORT gizmoViewport{};
            gizmoViewport.TopLeftX = marginX;
            gizmoViewport.TopLeftY = marginY;
            gizmoViewport.Width    = size;
            gizmoViewport.Height   = size;
            gizmoViewport.MinDepth = 0.0f;
            gizmoViewport.MaxDepth = 1.0f;

            D3D12_RECT gizmoScissor{};
            gizmoScissor.left   = static_cast<LONG>(marginX);
            gizmoScissor.top    = static_cast<LONG>(marginY);
            gizmoScissor.right  = static_cast<LONG>(marginX + size);
            gizmoScissor.bottom = static_cast<LONG>(marginY + size);

            commandList->RSSetViewports(1, &gizmoViewport);
            commandList->RSSetScissorRects(1, &gizmoScissor);

            // Clear depth within gizmo region so it renders on top of scene geometry while depth-testing against itself
            m_frameBuffer.ClearDepthScissor(commandList, gizmoScissor, 0.0f);

            // Extract camera's view rotation matrix and offset along view Z
            const auto rot = m_camera->GetViewMatrix().GetRotationMatrix();
            Maths::Mat4x4 gizmoView(
                static_cast<float>(rot.m[0][0]), static_cast<float>(rot.m[0][1]), static_cast<float>(rot.m[0][2]), 0.0f,
                static_cast<float>(rot.m[1][0]), static_cast<float>(rot.m[1][1]), static_cast<float>(rot.m[1][2]), 0.0f,
                static_cast<float>(rot.m[2][0]), static_cast<float>(rot.m[2][1]), static_cast<float>(rot.m[2][2]), 0.0f,
                0.0f,                            0.0f,                            3.0f,                            1.0f
            );

            // Square orthographic projection to prevent perspective distortion at corner
            const Maths::Mat4x4 gizmoProj = Maths::Mat4x4::Orthographic(2.8f, 2.8f, 0.1f, 10.0f);

            SceneConstantBuffer gizmoCb;
            gizmoCb.mvp          = gizmoView * gizmoProj;
            gizmoCb.world        = Maths::Mat4x4::Identity();
            // Unlit shading: zero diffuse contribution and unit ambient multiplier
            gizmoCb.ambientColor = Maths::Vec4::One();
            gizmoCb.fogColor     = Maths::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
            gizmoCb.fogParams    = Maths::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
            gizmoCb.lightCount   = 0;
            gizmoCb.isInstanced  = 0;

            const PipelineState* unlitPso = GetPipelineState("Unlit");
            if (unlitPso && unlitPso->GetPipelineState())
            {
                commandList->SetPipelineState(unlitPso->GetPipelineState());
            }

            const DynamicAllocation gizmoAlloc = m_dynamicConstantBuffer.Allocate(gizmoCb);
            commandList->SetGraphicsRootConstantBufferView(0, gizmoAlloc.gpuAddress);
            m_gizmoMesh->Draw(commandList);

            // Restore primary viewport and scissor rect from frame buffer
            commandList->RSSetViewports(1, &m_frameBuffer.GetViewport());
            commandList->RSSetScissorRects(1, &m_frameBuffer.GetScissorRect());
        }

        // Compute instantaneous and periodically averaged frame rate, targetting m_targetFps and no more
        const auto currentTime = std::chrono::high_resolution_clock::now();
        const float dt = std::chrono::duration<float>(currentTime - m_lastFrameTime).count();
        m_lastFrameTime = currentTime;

        const float maxTargetFps = m_targetFps;
        const float minFrameTimeMs = (maxTargetFps > 0.0f) ? (1000.0f / maxTargetFps) : 8.33f;

        if (dt > 0.0f && dt < 1.0f)
        {
            m_fpsTimeAccumulator += dt;
            m_fpsFrameCount++;

            // Periodically update diagnostic FPS metrics (every 250 ms) to eliminate sub-millisecond OS scheduling jitter and stabilise display
            if (m_fpsTimeAccumulator >= 0.25f)
            {
                const float measuredFps = static_cast<float>(m_fpsFrameCount) / m_fpsTimeAccumulator;
                const float measuredFrameTimeMs = (m_fpsTimeAccumulator / static_cast<float>(m_fpsFrameCount)) * 1000.0f;

                m_smoothedFps = std::min(measuredFps, maxTargetFps);
                m_smoothedFrameTimeMs = std::max(measuredFrameTimeMs, minFrameTimeMs);

                m_fpsTimeAccumulator = 0.0f;
                m_fpsFrameCount = 0;
            }
        }
        else if (dt >= 1.0f)
        {
            m_fpsTimeAccumulator = 0.0f;
            m_fpsFrameCount = 0;
        }

        // Render Diagnostic Text Overlay in the top-right corner (opposite the orientation gizmo)
        if (m_showOverlay && m_textOverlay && m_textOverlay->IsInitialised())
        {

            // Sum up total triangles and vertices across visible render items
            size_t visibleMeshCount = 0;
            size_t totalTriangles   = 0;
            size_t totalVertices    = 0;
            for (const auto& item : renderItems)
            {
                if (item.isVisible && item.mesh)
                {
                    ++visibleMeshCount;
                    totalTriangles += item.mesh->GetTriangleCount();
                    totalVertices  += item.mesh->GetVertexCount();
                }
            }

            // Total scene items include renderable meshes, the camera entity, and active light entities
            const size_t cameraCount = (m_camera != nullptr) ? 1 : 0;
            const size_t lightCount  = lights.size();
            const size_t sceneItemCount = (totalSceneItems > 0) ? totalSceneItems : (visibleMeshCount + cameraCount + lightCount);

            OverlayStatistics stats{};
            stats.gpuName       = m_gpuName;
            stats.fps           = std::min(m_smoothedFps, maxTargetFps);
            stats.ups           = updatesPerSecond;
            stats.targetFps     = m_targetFps;
            stats.targetUps     = (targetUps > 0.0f) ? targetUps : m_targetUps;
            stats.frameTimeMs   = std::max(m_smoothedFrameTimeMs, minFrameTimeMs);
            stats.sampleCount   = m_frameBuffer.GetSampleCount();
            stats.objectCount   = sceneItemCount;
            stats.itemCount     = sceneItemCount;
            stats.triangleCount = totalTriangles;
            stats.vertexCount   = totalVertices;
            stats.lightCount    = static_cast<uint32_t>(lightCount);

            const PipelineState* unlitPso = GetPipelineState("Unlit");
            if (unlitPso && unlitPso->GetPipelineState())
            {
                commandList->SetPipelineState(unlitPso->GetPipelineState());
            }

            m_textOverlay->Update(frameIndex, stats, m_width, m_height);
            m_textOverlay->Render(commandList, frameIndex, m_width, m_height, m_frameBuffer.GetDsvHandle());

            // Restore primary viewport and scissor rect from frame buffer
            commandList->RSSetViewports(1, &m_frameBuffer.GetViewport());
            commandList->RSSetScissorRects(1, &m_frameBuffer.GetScissorRect());
        }

        // Hardware MSAA resolve or blit off-screen frame buffer into destination swap chain back buffer
        m_frameBuffer.Resolve(commandList, backBuffer, D3D12_RESOURCE_STATE_PRESENT);

        // Execute command list on GPU and present frame
        m_commandContext.Execute(commandQueue, frameIndex);
        m_swapChain.Present(vSync);
    }

    void Renderer::InitialiseTextureResources(ID3D12Device* device, ID3D12CommandQueue* commandQueue)
    {
        if (!device || !commandQueue)
        {
            return;
        }

        // Allocate shader-visible SRV descriptor heap for scene textures
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.NumDescriptors = 48;
        heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        heapDesc.NodeMask       = 0;

        HR_CHECK(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_srvHeap)));
        m_srvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // List of terrain texture maps corresponding to shader slots t1 through t40
        const std::filesystem::path texDir = "resources/environment/terrain/textures";
        const std::vector<std::string> textureFiles = {
            // Low lying pasture and meadow
            "uncut_grass_oilpt20_4k_albedo.dds",
            "uncut_grass_oilpt20_4k_normal.dds",
            "uncut_grass_oilpt20_4k_roughness.dds",
            "uncut_grass_oilpt20_4k_ao.dds",

            // Mid slope fell turf
            "wild_grass_umjlabus_4k_albedo.dds",
            "wild_grass_umjlabus_4k_normal.dds",
            "wild_grass_umjlabus_4k_roughness.dds",
            "wild_grass_umjlabus_4k_ao.dds",

            // High plateau moorland
            "wild_grass_vbslfeqfw_4k_albedo.dds",
            "wild_grass_vbslfeqfw_4k_normal.dds",
            "wild_grass_vbslfeqfw_4k_roughness.dds",
            "wild_grass_vbslfeqfw_4k_ao.dds",

            // Dry thatch knolls and ridges
            "grass_dried_olqkj0_4k_albedo.dds",
            "grass_dried_olqkj0_4k_normal.dds",
            "grass_dried_olqkj0_4k_roughness.dds",
            "grass_dried_olqkj0_4k_ao.dds",

            // Crevice and sheer rock cliff
            "rock_cliff_vl3ibcxlw_4k_albedo.dds",
            "rock_cliff_vl3ibcxlw_4k_normal.dds",
            "rock_cliff_vl3ibcxlw_4k_roughness.dds",
            "rock_cliff_vl3ibcxlw_4k_ao.dds",

            // Displacement heightmaps for Parallax Occlusion Mapping
            "uncut_grass_oilpt20_4k_displacement.dds",
            "wild_grass_umjlabus_4k_displacement.dds",
            "wild_grass_vbslfeqfw_4k_displacement.dds",
            "grass_dried_olqkj0_4k_displacement.dds",
            "rock_cliff_vl3ibcxlw_4k_displacement.dds",

            // Alternative low-lying meadow (clover and grazed pasture)
            "uncut_grass_pftph0_4k_albedo.dds",
            "uncut_grass_pftph0_4k_normal.dds",
            "uncut_grass_pftph0_4k_roughness.dds",
            "uncut_grass_pftph0_4k_ao.dds",

            // Alternative mid-slope (fine moor-edge bent-grass)
            "wild_grass_umrmdgps_4k_albedo.dds",
            "wild_grass_umrmdgps_4k_normal.dds",
            "wild_grass_umrmdgps_4k_roughness.dds",
            "wild_grass_umrmdgps_4k_ao.dds",

            // Alternative high fell (weathered crag turf)
            "wild_grass_vbikagyn_4k_albedo.dds",
            "wild_grass_vbikagyn_4k_normal.dds",
            "wild_grass_vbikagyn_4k_roughness.dds",
            "wild_grass_vbikagyn_4k_ao.dds",

            // Alternative displacement heightmaps
            "uncut_grass_pftph0_4k_displacement.dds",
            "wild_grass_umrmdgps_4k_displacement.dds",
            "wild_grass_vbikagyn_4k_displacement.dds"
        };

        ComPtr<ID3D12CommandAllocator> uploadAlloc;
        HR_CHECK(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&uploadAlloc)));

        ComPtr<ID3D12GraphicsCommandList> uploadCmdList;
        HR_CHECK(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, uploadAlloc.Get(), nullptr, IID_PPV_ARGS(&uploadCmdList)));

        std::vector<ComPtr<ID3D12Resource>> stagingBuffers;
        stagingBuffers.reserve(textureFiles.size());
        m_terrainTextures.reserve(textureFiles.size());

        D3D12_CPU_DESCRIPTOR_HANDLE heapStart = m_srvHeap->GetCPUDescriptorHandleForHeapStart();

        for (size_t i = 0; i < textureFiles.size(); ++i)
        {
            const auto filePath = texDir / textureFiles[i];
            if (std::filesystem::exists(filePath))
            {
                try
                {
                    ComPtr<ID3D12Resource> staging;
                    auto texture = Texture::LoadFromDds(device, uploadCmdList.Get(), filePath, staging, textureFiles[i]);
                    if (texture)
                    {
                        D3D12_CPU_DESCRIPTOR_HANDLE destHandle = heapStart;
                        destHandle.ptr += i * m_srvDescriptorSize;
                        texture->CreateShaderResourceView(device, destHandle);

                        m_terrainTextures.push_back(texture);
                        stagingBuffers.push_back(staging);
                    }
                }
                catch (const std::exception& e)
                {
                    std::wcout << L"[Renderer] Warning: Could not load texture " << filePath.wstring().c_str()
                               << L": " << e.what() << L"\n";
                }
            }
        }

        // Initialise fallback texture at descriptor slot 40 (register t41)
        constexpr uint32_t defaultLidarPixel = 0xFF0000FF; // R=255 (AO=1.0), G=0 (crevice=0.0), B=0, A=255
        ComPtr<ID3D12Resource> fallbackStaging;
        auto fallbackLidarTexture = Texture::Create2D(
            device,
            uploadCmdList.Get(),
            &defaultLidarPixel,
            1,
            1,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            fallbackStaging,
            "DefaultLidarOcclusionFallback"
        );
        stagingBuffers.push_back(fallbackStaging);
        constexpr uint32_t lidarSrvSlot = 40;
        D3D12_CPU_DESCRIPTOR_HANDLE fallbackHandle = heapStart;
        fallbackHandle.ptr += lidarSrvSlot * m_srvDescriptorSize;
        fallbackLidarTexture->CreateShaderResourceView(device, fallbackHandle);
        m_lidarOcclusionTexture = fallbackLidarTexture;

        // Initialise fallback horizon angle textures at descriptor slots 41 and 42 (registers t42 and t43)
        // Default pixel 0x00000000 corresponds to zero horizon elevation (flat, unshadowed horizon)
        constexpr uint32_t defaultHorizonPixel = 0x00000000;
        ComPtr<ID3D12Resource> fallbackHorizon0Staging;
        auto fallbackHorizon0 = Texture::Create2D(
            device,
            uploadCmdList.Get(),
            &defaultHorizonPixel,
            1,
            1,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            fallbackHorizon0Staging,
            "DefaultLidarHorizon0Fallback"
        );
        stagingBuffers.push_back(fallbackHorizon0Staging);
        constexpr uint32_t lidarHorizon0Slot = 41;
        D3D12_CPU_DESCRIPTOR_HANDLE horizon0Handle = heapStart;
        horizon0Handle.ptr += lidarHorizon0Slot * m_srvDescriptorSize;
        fallbackHorizon0->CreateShaderResourceView(device, horizon0Handle);
        m_lidarHorizonTexture0 = fallbackHorizon0;

        ComPtr<ID3D12Resource> fallbackHorizon1Staging;
        auto fallbackHorizon1 = Texture::Create2D(
            device,
            uploadCmdList.Get(),
            &defaultHorizonPixel,
            1,
            1,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            fallbackHorizon1Staging,
            "DefaultLidarHorizon1Fallback"
        );
        stagingBuffers.push_back(fallbackHorizon1Staging);
        constexpr uint32_t lidarHorizon1Slot = 42;
        D3D12_CPU_DESCRIPTOR_HANDLE horizon1Handle = heapStart;
        horizon1Handle.ptr += lidarHorizon1Slot * m_srvDescriptorSize;
        fallbackHorizon1->CreateShaderResourceView(device, horizon1Handle);
        m_lidarHorizonTexture1 = fallbackHorizon1;

        // Initialise Cascaded Directional Shadows depth array SRV at descriptor slot 43 (register t44)
        constexpr uint32_t shadowSrvSlot = 43;
        D3D12_CPU_DESCRIPTOR_HANDLE shadowHandle = heapStart;
        shadowHandle.ptr += shadowSrvSlot * m_srvDescriptorSize;
        m_cascadedShadowMap.CreateShaderResourceView(device, shadowHandle);

        HR_CHECK(uploadCmdList->Close());
        ID3D12CommandList* lists[] = { uploadCmdList.Get() };
        commandQueue->ExecuteCommandLists(1, lists);

        // Synchronise with GPU queue to ensure staging buffers can be safely released
        ComPtr<ID3D12Fence> fence;
        HR_CHECK(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
        HR_CHECK(commandQueue->Signal(fence.Get(), 1));
        HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (event)
        {
            HR_CHECK(fence->SetEventOnCompletion(1, event));
            WaitForSingleObject(event, INFINITE);
            CloseHandle(event);
        }

        std::wcout << L"[Renderer] Loaded " << m_terrainTextures.size()
                   << L" terrain PBR textures into GPU descriptor table.\n";
    }

    void Renderer::SetLidarOcclusionMaps(
        ID3D12Device* device,
        ID3D12CommandQueue* commandQueue,
        const void* aoPixelData,
        const void* horizon0PixelData,
        const void* horizon1PixelData,
        uint32_t width,
        uint32_t height
    )
    {
        if (!device || !commandQueue || !aoPixelData || width == 0 || height == 0 || !m_srvHeap)
        {
            return;
        }

        ComPtr<ID3D12CommandAllocator> uploadAlloc;
        HR_CHECK(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&uploadAlloc)));

        ComPtr<ID3D12GraphicsCommandList> uploadCmdList;
        HR_CHECK(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, uploadAlloc.Get(), nullptr, IID_PPV_ARGS(&uploadCmdList)));

        std::vector<ComPtr<ID3D12Resource>> stagingBuffers;
        stagingBuffers.reserve(3);

        // Bind ambient occlusion and crevice depth map to descriptor slot 40 (register t41)
        ComPtr<ID3D12Resource> stagingAo;
        m_lidarOcclusionTexture = Texture::Create2D(
            device,
            uploadCmdList.Get(),
            aoPixelData,
            width,
            height,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            stagingAo,
            "LidarHorizonOcclusion"
        );
        stagingBuffers.push_back(stagingAo);

        D3D12_CPU_DESCRIPTOR_HANDLE heapStart = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_CPU_DESCRIPTOR_HANDLE aoHandle = heapStart;
        aoHandle.ptr += 40 * m_srvDescriptorSize;
        m_lidarOcclusionTexture->CreateShaderResourceView(device, aoHandle);

        // Bind directional horizon angles 0 to 3 to descriptor slot 41 (register t42)
        if (horizon0PixelData)
        {
            ComPtr<ID3D12Resource> stagingH0;
            m_lidarHorizonTexture0 = Texture::Create2D(
                device,
                uploadCmdList.Get(),
                horizon0PixelData,
                width,
                height,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                stagingH0,
                "LidarHorizonAngles0"
            );
            stagingBuffers.push_back(stagingH0);

            D3D12_CPU_DESCRIPTOR_HANDLE h0Handle = heapStart;
            h0Handle.ptr += 41 * m_srvDescriptorSize;
            m_lidarHorizonTexture0->CreateShaderResourceView(device, h0Handle);
        }

        // Bind directional horizon angles 4 to 7 to descriptor slot 42 (register t43)
        if (horizon1PixelData)
        {
            ComPtr<ID3D12Resource> stagingH1;
            m_lidarHorizonTexture1 = Texture::Create2D(
                device,
                uploadCmdList.Get(),
                horizon1PixelData,
                width,
                height,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                stagingH1,
                "LidarHorizonAngles1"
            );
            stagingBuffers.push_back(stagingH1);

            D3D12_CPU_DESCRIPTOR_HANDLE h1Handle = heapStart;
            h1Handle.ptr += 42 * m_srvDescriptorSize;
            m_lidarHorizonTexture1->CreateShaderResourceView(device, h1Handle);
        }

        HR_CHECK(uploadCmdList->Close());
        ID3D12CommandList* lists[] = { uploadCmdList.Get() };
        commandQueue->ExecuteCommandLists(1, lists);

        // Synchronise with GPU queue to ensure staging buffers can be safely released
        ComPtr<ID3D12Fence> fence;
        HR_CHECK(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
        HR_CHECK(commandQueue->Signal(fence.Get(), 1));
        HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (event)
        {
            HR_CHECK(fence->SetEventOnCompletion(1, event));
            WaitForSingleObject(event, INFINITE);
            CloseHandle(event);
        }

        std::wcout << L"[Renderer] Bound pre-baked LiDAR horizon occlusion, crevice depth, and directional horizon angle maps ("
                   << width << L"x" << height << L") to slots t41, t42, and t43.\n";
    }

    void Renderer::SetLidarOcclusionMap(
        ID3D12Device* device,
        ID3D12CommandQueue* commandQueue,
        const void* pixelData,
        uint32_t width,
        uint32_t height
    )
    {
        SetLidarOcclusionMaps(device, commandQueue, pixelData, nullptr, nullptr, width, height);
    }
}

