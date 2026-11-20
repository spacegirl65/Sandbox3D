// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Renderer.h"
#include "Shaders.h"

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
        m_hwnd   = hwnd;

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

        if (device && factory)
        {
            const LUID adapterLuid = device->GetAdapterLuid();
            ComPtr<IDXGIAdapter1> adapter1;
            for (UINT i = 0; SUCCEEDED(factory->EnumAdapterByLuid(adapterLuid, IID_PPV_ARGS(&adapter1))); ++i)
            {
                adapter1.As(&m_dxgiAdapter);
                break;
            }
        }

        // Initialise SwapChain & CommandContext
        m_swapChain.Initialise(factory, device, commandQueue, hwnd, m_width, m_height);
        m_commandContext.Initialise(device);

        // Initialise off-screen multisampled linear HDR frame buffer
        constexpr DXGI_FORMAT hdrFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
        m_frameBuffer.Initialise(
            device,
            m_width,
            m_height,
            hdrFormat,
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
            hdrFormat,
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
            hdrFormat,
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

        // Post-tonemap unlit overlay mesh shader (source/Shaders/Unlit.hlsl): always-on-top wireframe and translucent boundary planes
        m_unlitPipelineState.Initialise(
            device,
            m_pipelineState.GetRootSignature(),
            unlitVs,
            unlitPs,
            m_swapChain.GetFormat(),
            DXGI_FORMAT_UNKNOWN,
            1,
            0,
            /* depthWrite = */ false,
            D3D12_COMPARISON_FUNC_ALWAYS,
            D3D12_CULL_MODE_NONE
        );

        // Post-tonemap orientation gizmo shader (source/Shaders/Unlit.hlsl): self-occluding with Reverse-Z depth testing
        m_gizmoPipelineState.Initialise(
            device,
            m_pipelineState.GetRootSignature(),
            unlitVs,
            unlitPs,
            m_swapChain.GetFormat(),
            DXGI_FORMAT_D32_FLOAT,
            1,
            0,
            /* depthWrite = */ true,
            D3D12_COMPARISON_FUNC_GREATER_EQUAL,
            D3D12_CULL_MODE_NONE
        );

        // Post-tonemap 2D diagnostic overlay pipeline targeting the single-sampled swap chain back buffer
        m_overlayPipelineState.Initialise(
            device,
            m_pipelineState.GetRootSignature(),
            unlitVs,
            unlitPs,
            m_swapChain.GetFormat(),
            DXGI_FORMAT_UNKNOWN,
            1,
            0,
            /* depthWrite = */ false,
            D3D12_COMPARISON_FUNC_ALWAYS,
            D3D12_CULL_MODE_NONE
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
                hdrFormat,
                DXGI_FORMAT_D32_FLOAT,
                m_frameBuffer.GetSampleCount(),
                0,
                /* depthWrite = */ false,
                D3D12_COMPARISON_FUNC_GREATER_EQUAL,
                D3D12_CULL_MODE_NONE
            );
        }

        // Centralised HDR post-processing and tone reproduction shader (source/Shaders/PostProcess.hlsl)
        Shader postProcessVs;
        Shader postProcessPs;
        Shader bloomPrefilterPs;
        Shader bloomDownsamplePs;
        Shader bloomUpsamplePs;
        Shader luminanceMeterPs;
        Shader exposureAdaptPs;
        const std::filesystem::path postProcessPath = "source/Shaders/PostProcess.hlsl";
        if (std::filesystem::exists(postProcessPath))
        {
            postProcessVs.CompileFromFile(postProcessPath, "VSMain", ShaderStage::Vertex);
            postProcessPs.CompileFromFile(postProcessPath, "PSMain", ShaderStage::Pixel);
            bloomPrefilterPs.CompileFromFile(postProcessPath, "BloomPrefilterPS", ShaderStage::Pixel);
            bloomDownsamplePs.CompileFromFile(postProcessPath, "BloomDownsamplePS", ShaderStage::Pixel);
            bloomUpsamplePs.CompileFromFile(postProcessPath, "BloomUpsamplePS", ShaderStage::Pixel);
            luminanceMeterPs.CompileFromFile(postProcessPath, "LuminanceMeterPS", ShaderStage::Pixel);
            exposureAdaptPs.CompileFromFile(postProcessPath, "ExposureAdaptPS", ShaderStage::Pixel);
        }
        else
        {
            const std::string postProcessSource = std::string(s_embeddedSceneBuffers) + s_embeddedPostProcessVertexStage + s_embeddedPostProcessPixelStage;
            postProcessVs.CompileFromSource(postProcessSource, "EmbeddedPostProcess.hlsl", "VSMain", ShaderStage::Vertex);
            postProcessPs.CompileFromSource(postProcessSource, "EmbeddedPostProcess.hlsl", "PSMain", ShaderStage::Pixel);
        }

        m_postProcessPipelineState.Initialise(
            device,
            m_pipelineState.GetRootSignature(),
            postProcessVs,
            postProcessPs,
            m_swapChain.GetFormat(),
            DXGI_FORMAT_UNKNOWN,
            1,
            0,
            /* depthWrite = */ false,
            D3D12_COMPARISON_FUNC_ALWAYS,
            D3D12_CULL_MODE_NONE
        );

        if (bloomPrefilterPs.GetBufferSize() > 0 && bloomDownsamplePs.GetBufferSize() > 0 && bloomUpsamplePs.GetBufferSize() > 0)
        {
            m_bloomPrefilterPipelineState.Initialise(
                device,
                m_pipelineState.GetRootSignature(),
                postProcessVs,
                bloomPrefilterPs,
                hdrFormat,
                DXGI_FORMAT_UNKNOWN,
                1,
                0,
                /* depthWrite = */ false,
                D3D12_COMPARISON_FUNC_ALWAYS,
                D3D12_CULL_MODE_NONE,
                /* additiveBlend = */ false
            );

            m_bloomDownsamplePipelineState.Initialise(
                device,
                m_pipelineState.GetRootSignature(),
                postProcessVs,
                bloomDownsamplePs,
                hdrFormat,
                DXGI_FORMAT_UNKNOWN,
                1,
                0,
                /* depthWrite = */ false,
                D3D12_COMPARISON_FUNC_ALWAYS,
                D3D12_CULL_MODE_NONE,
                /* additiveBlend = */ false
            );

            m_bloomUpsamplePipelineState.Initialise(
                device,
                m_pipelineState.GetRootSignature(),
                postProcessVs,
                bloomUpsamplePs,
                hdrFormat,
                DXGI_FORMAT_UNKNOWN,
                1,
                0,
                /* depthWrite = */ false,
                D3D12_COMPARISON_FUNC_ALWAYS,
                D3D12_CULL_MODE_NONE,
                /* additiveBlend = */ true
            );
        }

        if (luminanceMeterPs.GetBufferSize() > 0 && exposureAdaptPs.GetBufferSize() > 0)
        {
            m_luminanceMeterPipelineState.Initialise(
                device,
                m_pipelineState.GetRootSignature(),
                postProcessVs,
                luminanceMeterPs,
                DXGI_FORMAT_R16G16_FLOAT,
                DXGI_FORMAT_UNKNOWN,
                1,
                0,
                /* depthWrite = */ false,
                D3D12_COMPARISON_FUNC_ALWAYS,
                D3D12_CULL_MODE_NONE,
                /* additiveBlend = */ false
            );

            m_exposureAdaptPipelineState.Initialise(
                device,
                m_pipelineState.GetRootSignature(),
                postProcessVs,
                exposureAdaptPs,
                DXGI_FORMAT_R32_FLOAT,
                DXGI_FORMAT_UNKNOWN,
                1,
                0,
                /* depthWrite = */ false,
                D3D12_COMPARISON_FUNC_ALWAYS,
                D3D12_CULL_MODE_NONE,
                /* additiveBlend = */ false
            );
        }

        // Initialise Dynamic Constant Buffer Ring Allocator, Orientation Gizmo, and Diagnostic Text Overlay
        m_dynamicConstantBuffer.Initialise(device, DynamicUploadBuffer::DefaultPageSize, SwapChain::BufferCount);
        m_gizmoMesh = Mesh::CreateCoordinateAxes(device);

        m_debugOverlay = std::make_unique<Engine::DebugOverlay>();
        m_debugOverlay->Initialise(device);

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
    }

    void Renderer::Shutdown(ID3D12CommandQueue* commandQueue) noexcept
    {
        if (!m_isInitialised)
        {
            return;
        }

        s_initialised = false;

        if (m_debugOverlay)
        {
            m_debugOverlay->Shutdown();
            m_debugOverlay.reset();
        }

        m_cascadedShadowMap.Shutdown();
        m_skyboxTexture.reset();
        for (auto& mip : m_bloomMips)
        {
            mip.texture.Reset();
            mip.width = 0;
            mip.height = 0;
        }
        m_luminanceMeterTexture.Reset();
        m_exposureTarget.Reset();
        m_exposureHistory.Reset();
        m_gizmoDepthBuffer.Reset();
        m_gizmoDsvHeap.Reset();
        m_bloomRtvHeap.Reset();
        m_srvHeap.Reset();
        m_commandContext.Shutdown(commandQueue);
        m_dynamicConstantBuffer.Shutdown();
        m_gizmoMesh.reset();
        m_frameBuffer.Shutdown();
        m_swapChain.Shutdown();
        m_renderQueue.Clear();
        m_camera = &m_fallbackCamera;
        m_pipelineState = {};
        m_depthPipelineState = {};
        m_skyPipelineState = {};
        m_postProcessPipelineState = {};
        m_bloomPrefilterPipelineState = {};
        m_bloomDownsamplePipelineState = {};
        m_bloomUpsamplePipelineState = {};
        m_luminanceMeterPipelineState = {};
        m_exposureAdaptPipelineState = {};
        m_overlayPipelineState = {};
        m_unlitPipelineState = {};
        m_gizmoPipelineState = {};
        m_pipelineStates.clear();
        m_dxgiAdapter.Reset();
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
        if (shaderName == "Gizmo")
        {
            return &m_gizmoPipelineState;
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
        if (shaderName == "Gizmo")
        {
            return &m_gizmoPipelineState;
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
        UpdatePostProcessSrv(device);
        CreateBloomResources(device);
        CreateGizmoDepthBuffer(device);
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
        // Synchronise CPU frame submission with display presentation cadence via DXGI waitable object
        m_swapChain.WaitForFrameLatency();

        // Compute instantaneous and periodically averaged frame rate synchronized to display cadence
        const auto currentTime = std::chrono::high_resolution_clock::now();
        const float dt = std::chrono::duration<float>(currentTime - m_lastFrameTime).count();
        m_lastFrameTime = currentTime;

        if (dt > 0.0f && dt < 1.0f)
        {
            m_fpsTimeAccumulator += dt;
            m_fpsFrameCount++;

            // Periodically update diagnostic FPS and hardware telemetry metrics (every 250 ms) to eliminate scheduling jitter and CPU overhead
            if (m_fpsTimeAccumulator >= 0.25f || m_forceTelemetryUpdate)
            {
                if (m_fpsTimeAccumulator > 0.0f && m_fpsFrameCount > 0)
                {
                    m_smoothedFps = static_cast<float>(m_fpsFrameCount) / m_fpsTimeAccumulator;
                    m_smoothedFrameTimeMs = (m_fpsTimeAccumulator / static_cast<float>(m_fpsFrameCount)) * 1000.0f;
                }

                m_fpsTimeAccumulator = 0.0f;
                m_fpsFrameCount = 0;

                // Query video memory info and window presentation mode only during the periodic refresh window
                if (m_showOverlay)
                {
                    if (m_dxgiAdapter)
                    {
                        DXGI_QUERY_VIDEO_MEMORY_INFO localMemInfo{};
                        if (SUCCEEDED(m_dxgiAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &localMemInfo)))
                        {
                            m_cachedVramLocalUsedBytes   = localMemInfo.CurrentUsage;
                            m_cachedVramLocalBudgetBytes = localMemInfo.Budget;
                        }
                    }

                    if (m_swapChain.IsFullscreen())
                    {
                        m_cachedWindowMode = "Fullscreen";
                    }
                    else if (m_hwnd)
                    {
                        const LONG_PTR style = GetWindowLongPtrW(m_hwnd, GWL_STYLE);
                        if ((style & WS_POPUP) != 0 && (style & WS_CAPTION) == 0)
                        {
                            m_cachedWindowMode = "Borderless";
                        }
                        else
                        {
                            m_cachedWindowMode = "Windowed";
                        }
                    }
                    else
                    {
                        m_cachedWindowMode = "Windowed";
                    }

                    // Sum up total triangles and vertices across visible render items
                    m_cachedVisibleMeshCount = 0;
                    m_cachedTotalTriangles   = 0;
                    m_cachedTotalVertices    = 0;
                    for (const auto& item : renderItems)
                    {
                        if (item.isVisible && item.mesh)
                        {
                            ++m_cachedVisibleMeshCount;
                            m_cachedTotalTriangles += item.mesh->GetTriangleCount();
                            m_cachedTotalVertices  += item.mesh->GetVertexCount();
                        }
                    }

                    m_forceTelemetryUpdate = false;
                }
            }
        }
        else if (dt >= 1.0f)
        {
            m_fpsTimeAccumulator = 0.0f;
            m_fpsFrameCount = 0;
        }

        const UINT frameIndex = m_swapChain.GetCurrentBackBufferIndex();
        m_commandContext.BeginFrame(frameIndex);
        m_dynamicConstantBuffer.BeginFrame(frameIndex);

        ID3D12GraphicsCommandList* const commandList = m_commandContext.GetCommandList();
        ID3D12Resource* const backBuffer = m_swapChain.GetCurrentRenderTarget();

        // Set root signature
        commandList->SetGraphicsRootSignature(m_pipelineState.GetRootSignature());

        // Bind texture descriptor heap and tables across vertex and pixel stages
        if (m_srvHeap)
        {
            ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
            commandList->SetDescriptorHeaps(1, heaps);
            const D3D12_GPU_DESCRIPTOR_HANDLE gpuHeapStart = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
            commandList->SetGraphicsRootDescriptorTable(2, gpuHeapStart);
            commandList->SetGraphicsRootDescriptorTable(3, gpuHeapStart);
        }

        const Maths::Vec3D cameraPosition = m_camera ? m_camera->GetPosition() : Maths::Vec3D::Zero();
        const Maths::BoundingFrustumD cullingFrustum = m_camera ? m_camera->GetFrustumD() : Maths::BoundingFrustumD();
        const Maths::BoundingFrustumD* pCullingFrustum = m_camera ? &cullingFrustum : nullptr;

        // Build, cull against active view frustum, and sort the render queue into state-minimised batches
        m_renderQueue.Build(renderItems, cameraPosition, m_pipelineStates, m_pipelineState, &m_unlitPipelineState, pCullingFrustum);

        // Pre-populate per-frame common scene lighting and atmospheric parameters
        SceneConstantBuffer commonCbData{};
        commonCbData.ambientColor     = m_ambientColor;
        commonCbData.fogColor         = m_fogColor;
        commonCbData.fogParams        = m_fogParams;
        commonCbData.fogParams.w      = static_cast<float>(cameraPosition.y);
        commonCbData.cameraPosition   = Maths::Vec4(
            static_cast<float>(cameraPosition.x),
            static_cast<float>(cameraPosition.y),
            static_cast<float>(cameraPosition.z),
            static_cast<float>(cameraPosition.y)
        );
        commonCbData.rayleighParams   = m_rayleighParams;
        commonCbData.mieParams        = m_mieParams;
        commonCbData.ozoneParams      = m_ozoneParams;
        commonCbData.atmosphereParams = m_atmosphereParams;
        commonCbData.adaptationParams.z = 1;

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
        uint32_t totalDrawCalls = 0;

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

                // Bypass continuous heightfield terrain and unlit materials from the depth pre-pass
                if (batch.material && (batch.material->IsTerrain() || batch.material->IsUnlit()))
                {
                    continue;
                }

                if (batch.mesh != depthBoundMesh)
                {
                    depthBoundMesh = batch.mesh;
                    depthBoundMesh->Bind(commandList);
                }

                const size_t instanceCount = batch.items.size();
                const bool isTerrain = batch.material && batch.material->IsTerrain();
                const bool canUseDirectPath = (instanceCount == 1 && isTerrain && batch.items[0]->colorTint == Maths::Vec4::One());

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
                    ++totalDrawCalls;
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

                        if (item->material && !item->material->IsTerrain())
                        {
                            constexpr float defaultAmbientOcclusion = 1.0f;
                            dest[i].materialParams = Maths::Vec4(
                                item->material->GetRoughness(),
                                item->material->GetMetallic(),
                                item->material->GetSpecular(),
                                defaultAmbientOcclusion
                            );
                            dest[i].colorTint = item->colorTint * item->material->GetAlbedo();
                        }
                        else
                        {
                            dest[i].morphParams = item->morphParams;
                        }
                    }

                    commandList->SetGraphicsRootShaderResourceView(1, instAlloc.gpuAddress);
                    depthBoundMesh->DrawBound(commandList, static_cast<uint32_t>(instanceCount), 0);
                    ++totalDrawCalls;
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
            ID3D12DescriptorHeap* activeHeap = m_srvHeap.Get();
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

                // Material descriptor heap binding: switch when material provides a dedicated heap
                ID3D12DescriptorHeap* const targetHeap = (batch.material && batch.material->HasDescriptorHeap())
                    ? batch.material->GetDescriptorHeap()
                    : m_srvHeap.Get();

                if (targetHeap && targetHeap != activeHeap)
                {
                    activeHeap = targetHeap;
                    ID3D12DescriptorHeap* heaps[] = { activeHeap };
                    commandList->SetDescriptorHeaps(1, heaps);
                    const D3D12_GPU_DESCRIPTOR_HANDLE gpuHeapStart = activeHeap->GetGPUDescriptorHandleForHeapStart();
                    commandList->SetGraphicsRootDescriptorTable(2, gpuHeapStart);
                    commandList->SetGraphicsRootDescriptorTable(3, gpuHeapStart);
                }

                // Input Assembler binding: bind buffers only when mesh geometry changes
                if (batch.mesh != currentBoundMesh)
                {
                    currentBoundMesh = batch.mesh;
                    currentBoundMesh->Bind(commandList);
                }

                const size_t instanceCount = batch.items.size();
                const bool isTerrain = batch.material && batch.material->IsTerrain();
                const bool canUseDirectPath = (instanceCount == 1 && isTerrain && batch.items[0]->colorTint == Maths::Vec4::One());

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
                    ++totalDrawCalls;
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

                        if (item->material && !item->material->IsTerrain())
                        {
                            constexpr float defaultAmbientOcclusion = 1.0f;
                            dest[i].materialParams = Maths::Vec4(
                                item->material->GetRoughness(),
                                item->material->GetMetallic(),
                                item->material->GetSpecular(),
                                defaultAmbientOcclusion
                            );
                            dest[i].colorTint = item->colorTint * item->material->GetAlbedo();
                        }
                        else
                        {
                            dest[i].morphParams = item->morphParams;
                        }
                    }

                    commandList->SetGraphicsRootShaderResourceView(1, instAlloc.gpuAddress);
                    currentBoundMesh->DrawBound(commandList, static_cast<uint32_t>(instanceCount), 0);
                    ++totalDrawCalls;
                }
            }

            if (activeHeap != m_srvHeap.Get() && m_srvHeap)
            {
                ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
                commandList->SetDescriptorHeaps(1, heaps);
                const D3D12_GPU_DESCRIPTOR_HANDLE gpuHeapStart = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
                commandList->SetGraphicsRootDescriptorTable(2, gpuHeapStart);
                commandList->SetGraphicsRootDescriptorTable(3, gpuHeapStart);
            }
        };

        // Execute opaque batches, followed by transparent batches
        executeBatches(m_renderQueue.GetOpaqueBatches());
        executeBatches(m_renderQueue.GetTransparentBatches());

        // Reverse-Z sky dome and solar disc pass
        if (m_skyPipelineState.GetPipelineState())
        {
            commandList->SetPipelineState(m_skyPipelineState.GetPipelineState());

            if (m_srvHeap)
            {
                ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
                commandList->SetDescriptorHeaps(1, heaps);
                const D3D12_GPU_DESCRIPTOR_HANDLE gpuHeapStart = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
                commandList->SetGraphicsRootDescriptorTable(2, gpuHeapStart);
            }

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
            ++totalDrawCalls;
        }

        // Resolve off-screen multisampled HDR frame buffer into single-sampled HDR target for post-processing
        m_frameBuffer.Resolve(commandList);

        // Execute multi-pass HDR optical bloom mip pyramid when bloom pipelines and targets are ready
        const bool bloomReady =
            m_bloomIntensity > 0.0f &&
            m_srvHeap &&
            m_bloomRtvHeap &&
            m_bloomMips[0].texture &&
            m_bloomPrefilterPipelineState.GetPipelineState() &&
            m_bloomDownsamplePipelineState.GetPipelineState() &&
            m_bloomUpsamplePipelineState.GetPipelineState();

        if (bloomReady)
        {
            ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
            commandList->SetDescriptorHeaps(1, heaps);
            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            SceneConstantBuffer bloomCb = commonCbData;
            bloomCb.isInstanced = 0;
            const DynamicAllocation bloomCbAlloc = m_dynamicConstantBuffer.Allocate(bloomCb);
            commandList->SetGraphicsRootConstantBufferView(0, bloomCbAlloc.gpuAddress);

            const D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
            const D3D12_CPU_DESCRIPTOR_HANDLE rtvCpuStart = m_bloomRtvHeap->GetCPUDescriptorHandleForHeapStart();

            auto transitionBloomMip = [commandList](ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
            {
                D3D12_RESOURCE_BARRIER barrier{};
                barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                barrier.Transition.pResource   = resource;
                barrier.Transition.StateBefore = before;
                barrier.Transition.StateAfter  = after;
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                commandList->ResourceBarrier(1, &barrier);
            };

            auto drawBloomTarget = [&](uint32_t targetMipIndex, uint32_t srvTableOffset)
            {
                const BloomMip& targetMip = m_bloomMips[targetMipIndex];
                transitionBloomMip(targetMip.texture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

                D3D12_CPU_DESCRIPTOR_HANDLE targetRtv = rtvCpuStart;
                targetRtv.ptr += static_cast<SIZE_T>(targetMipIndex) * m_rtvDescriptorSize;
                commandList->OMSetRenderTargets(1, &targetRtv, FALSE, nullptr);

                D3D12_VIEWPORT mipViewport{};
                mipViewport.TopLeftX = 0.0f;
                mipViewport.TopLeftY = 0.0f;
                mipViewport.Width    = static_cast<float>(targetMip.width);
                mipViewport.Height   = static_cast<float>(targetMip.height);
                mipViewport.MinDepth = 0.0f;
                mipViewport.MaxDepth = 1.0f;

                D3D12_RECT mipScissor{};
                mipScissor.left   = 0;
                mipScissor.top    = 0;
                mipScissor.right  = static_cast<LONG>(targetMip.width);
                mipScissor.bottom = static_cast<LONG>(targetMip.height);

                commandList->RSSetViewports(1, &mipViewport);
                commandList->RSSetScissorRects(1, &mipScissor);

                D3D12_GPU_DESCRIPTOR_HANDLE tableHandle = srvGpuStart;
                tableHandle.ptr += static_cast<UINT64>(srvTableOffset) * m_srvDescriptorSize;
                commandList->SetGraphicsRootDescriptorTable(2, tableHandle);

                commandList->DrawInstanced(3, 1, 0, 0);
                ++totalDrawCalls;

                transitionBloomMip(targetMip.texture.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            };

            // Prefilter full-resolution HDR scene (slot 32 -> t33 at offset 0) into half-resolution bloom mip 0
            commandList->SetPipelineState(m_bloomPrefilterPipelineState.GetPipelineState());
            drawBloomTarget(0, 0);

            // Progressively downsample through the bloom mip chain (mip i at offset 1 + i -> mip i + 1)
            commandList->SetPipelineState(m_bloomDownsamplePipelineState.GetPipelineState());
            for (uint32_t mipIndex = 0; mipIndex + 1 < BloomMipCount; ++mipIndex)
            {
                drawBloomTarget(mipIndex + 1, 1u + mipIndex);
            }

            // Progressively upsample and additively accumulate back to mip 0 (mip i + 1 at offset 2 + i -> mip i)
            commandList->SetPipelineState(m_bloomUpsamplePipelineState.GetPipelineState());
            for (uint32_t mipIndex = BloomMipCount - 1; mipIndex > 0; --mipIndex)
            {
                const uint32_t targetMipIndex = mipIndex - 1;
                drawBloomTarget(targetMipIndex, 2u + targetMipIndex);
            }
        }

        // Execute centre-weighted log-luminance metering and temporal eye-adaptation passes
        const bool autoExposureReady =
            m_enableAutoExposure &&
            m_srvHeap &&
            m_bloomRtvHeap &&
            m_luminanceMeterTexture &&
            m_exposureTarget &&
            m_exposureHistory &&
            m_luminanceMeterPipelineState.GetPipelineState() &&
            m_exposureAdaptPipelineState.GetPipelineState();

        if (autoExposureReady)
        {
            auto transitionResource = [commandList](ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
            {
                D3D12_RESOURCE_BARRIER barrier{};
                barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                barrier.Transition.pResource   = resource;
                barrier.Transition.StateBefore = before;
                barrier.Transition.StateAfter  = after;
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                commandList->ResourceBarrier(1, &barrier);
            };

            ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
            commandList->SetDescriptorHeaps(1, heaps);
            const D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
            commandList->SetGraphicsRootDescriptorTable(2, srvGpuStart);
            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            const float clampedDt = std::clamp(dt, 0.0001f, 0.25f);
            SceneConstantBuffer exposureCb = commonCbData;
            exposureCb.isInstanced        = 0;
            exposureCb.exposureParams     = Maths::Vec4(m_exposureCompensation, clampedDt, m_minAutoExposure, m_maxAutoExposure);
            exposureCb.adaptationParams.y = m_adaptationSpeed;
            const DynamicAllocation exposureCbAlloc = m_dynamicConstantBuffer.Allocate(exposureCb);
            commandList->SetGraphicsRootConstantBufferView(0, exposureCbAlloc.gpuAddress);

            const D3D12_CPU_DESCRIPTOR_HANDLE rtvCpuStart = m_bloomRtvHeap->GetCPUDescriptorHandleForHeapStart();

            // Render 16x16 centre-weighted log2 luminance meter from resolved HDR scene (t33)
            transitionResource(m_luminanceMeterTexture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
            D3D12_CPU_DESCRIPTOR_HANDLE meterRtv = rtvCpuStart;
            meterRtv.ptr += static_cast<SIZE_T>(BloomMipCount) * m_rtvDescriptorSize;
            commandList->OMSetRenderTargets(1, &meterRtv, FALSE, nullptr);

            D3D12_VIEWPORT meterViewport{ 0.0f, 0.0f, static_cast<float>(LuminanceMeterSize), static_cast<float>(LuminanceMeterSize), 0.0f, 1.0f };
            D3D12_RECT meterScissor{ 0, 0, static_cast<LONG>(LuminanceMeterSize), static_cast<LONG>(LuminanceMeterSize) };
            commandList->RSSetViewports(1, &meterViewport);
            commandList->RSSetScissorRects(1, &meterScissor);
            commandList->SetPipelineState(m_luminanceMeterPipelineState.GetPipelineState());
            commandList->DrawInstanced(3, 1, 0, 0);
            ++totalDrawCalls;
            transitionResource(m_luminanceMeterTexture.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

            // Reduce 16x16 luminance meter (t39) and previous exposure history (t40) into 1x1 adapted exposure target
            D3D12_CPU_DESCRIPTOR_HANDLE exposureRtv = rtvCpuStart;
            exposureRtv.ptr += static_cast<SIZE_T>(BloomMipCount + 1u) * m_rtvDescriptorSize;
            commandList->OMSetRenderTargets(1, &exposureRtv, FALSE, nullptr);

            D3D12_VIEWPORT exposureViewport{ 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };
            D3D12_RECT exposureScissor{ 0, 0, 1, 1 };
            commandList->RSSetViewports(1, &exposureViewport);
            commandList->RSSetScissorRects(1, &exposureScissor);
            commandList->SetPipelineState(m_exposureAdaptPipelineState.GetPipelineState());
            commandList->DrawInstanced(3, 1, 0, 0);
            ++totalDrawCalls;

            // Copy newly adapted 1x1 exposure scalar into the persistent SRV history resource (t40)
            D3D12_RESOURCE_BARRIER copyPreBarriers[2]{};
            copyPreBarriers[0].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            copyPreBarriers[0].Transition.pResource   = m_exposureTarget.Get();
            copyPreBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            copyPreBarriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE;
            copyPreBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            copyPreBarriers[1].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            copyPreBarriers[1].Transition.pResource   = m_exposureHistory.Get();
            copyPreBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            copyPreBarriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
            copyPreBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            commandList->ResourceBarrier(2, copyPreBarriers);

            commandList->CopyResource(m_exposureHistory.Get(), m_exposureTarget.Get());

            D3D12_RESOURCE_BARRIER copyPostBarriers[2]{};
            copyPostBarriers[0].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            copyPostBarriers[0].Transition.pResource   = m_exposureTarget.Get();
            copyPostBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
            copyPostBarriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
            copyPostBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            copyPostBarriers[1].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            copyPostBarriers[1].Transition.pResource   = m_exposureHistory.Get();
            copyPostBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            copyPostBarriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            copyPostBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            commandList->ResourceBarrier(2, copyPostBarriers);
        }

        // Transition swap chain back buffer from PRESENT to RENDER_TARGET for post-processing and UI overlay
        D3D12_RESOURCE_BARRIER backBufferToRt{};
        backBufferToRt.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        backBufferToRt.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        backBufferToRt.Transition.pResource   = backBuffer;
        backBufferToRt.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        backBufferToRt.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
        backBufferToRt.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &backBufferToRt);

        const D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv = m_swapChain.GetCurrentRtvHandle();
        commandList->OMSetRenderTargets(1, &backBufferRtv, FALSE, nullptr);
        commandList->RSSetViewports(1, &m_frameBuffer.GetViewport());
        commandList->RSSetScissorRects(1, &m_frameBuffer.GetScissorRect());

        // Execute centralised post-processing pass (bloom, optical glare, auto-exposure, ACES tone mapping, and dithering)
        if (m_postProcessPipelineState.GetPipelineState())
        {
            commandList->SetPipelineState(m_postProcessPipelineState.GetPipelineState());

            if (m_srvHeap)
            {
                ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
                commandList->SetDescriptorHeaps(1, heaps);
                const D3D12_GPU_DESCRIPTOR_HANDLE gpuHeapStart = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
                commandList->SetGraphicsRootDescriptorTable(2, gpuHeapStart);
            }

            constexpr uint32_t postProcessFrameWrap = 4096u;
            const float postProcessFrameSeed = static_cast<float>(m_postProcessFrameIndex % postProcessFrameWrap);
            ++m_postProcessFrameIndex;

            SceneConstantBuffer postProcessCb = commonCbData;
            postProcessCb.mvp                = m_camera ? m_camera->GetProjectionMatrix() : Maths::Mat4x4::Identity();
            postProcessCb.world              = m_camera ? Maths::Mat4x4(m_camera->GetViewMatrix().Inverted()) : Maths::Mat4x4::Identity();
            postProcessCb.isInstanced        = 0;
            postProcessCb.exposureParams     = Maths::Vec4(
                m_exposureCompensation,
                postProcessFrameSeed,
                autoExposureReady ? m_minAutoExposure : 0.0f,
                m_maxAutoExposure
            );
            postProcessCb.adaptationParams.w = bloomReady ? m_bloomIntensity : 0.0f;
            postProcessCb.colorGradeParams   = m_colorGradeParams;
            postProcessCb.opticsParams       = m_opticsParams;
            const DynamicAllocation postProcessAlloc = m_dynamicConstantBuffer.Allocate(postProcessCb);
            commandList->SetGraphicsRootConstantBufferView(0, postProcessAlloc.gpuAddress);

            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            commandList->DrawInstanced(3, 1, 0, 0);
            ++totalDrawCalls;
        }

        // Release HDR render target back to RENDER_TARGET state when single-sampled
        m_frameBuffer.PostProcessFinished(commandList);

        // Execute post-tonemap unlit overlay batches (wireframe edges and boundary planes) unaffected by post-processing
        currentPso       = nullptr;
        currentBoundMesh = nullptr;
        executeBatches(m_renderQueue.GetUnlitBatches());

        // Render post-tonemap World-Space Orientation Gizmo in the top-left corner unaffected by bloom, exposure, or lens optics
        if (m_showGizmo && m_gizmoMesh && m_gizmoMesh->IsInitialised() && m_gizmoDsvHeap && m_gizmoDepthBuffer)
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

            const D3D12_CPU_DESCRIPTOR_HANDLE gizmoDsv = m_gizmoDsvHeap->GetCPUDescriptorHandleForHeapStart();
            commandList->OMSetRenderTargets(1, &backBufferRtv, FALSE, &gizmoDsv);
            commandList->RSSetViewports(1, &gizmoViewport);
            commandList->RSSetScissorRects(1, &gizmoScissor);

            // Clear Reverse-Z depth within the gizmo region for clean self-occlusion over the post-processed scene
            commandList->ClearDepthStencilView(gizmoDsv, D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 1, &gizmoScissor);

            constexpr float gizmoCameraDistance = 4.05f;
            constexpr float gizmoFovDegrees     = 38.0f;

            const auto rot = m_camera->GetViewMatrix().GetRotationMatrix();
            Maths::Mat4x4 gizmoView(
                static_cast<float>(rot.m[0][0]), static_cast<float>(rot.m[0][1]), static_cast<float>(rot.m[0][2]), 0.0f,
                static_cast<float>(rot.m[1][0]), static_cast<float>(rot.m[1][1]), static_cast<float>(rot.m[1][2]), 0.0f,
                static_cast<float>(rot.m[2][0]), static_cast<float>(rot.m[2][1]), static_cast<float>(rot.m[2][2]), 0.0f,
                0.0f,                            0.0f,                            gizmoCameraDistance,             1.0f
            );

            // Perspective projection provides natural foreshortening between front-facing and receding gizmo axes
            const Maths::Mat4x4 gizmoProj = Maths::Mat4x4::Perspective(Maths::ToRadians(gizmoFovDegrees), 1.0f, 0.1f, 10.0f);

            SceneConstantBuffer gizmoCb;
            gizmoCb.mvp          = gizmoView * gizmoProj;
            gizmoCb.world        = Maths::Mat4x4::Identity();
            gizmoCb.ambientColor = Maths::Vec4::One();
            gizmoCb.fogColor     = Maths::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
            gizmoCb.fogParams    = Maths::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
            gizmoCb.lightCount   = 0;
            gizmoCb.isInstanced  = 0;

            if (m_gizmoPipelineState.GetPipelineState())
            {
                commandList->SetPipelineState(m_gizmoPipelineState.GetPipelineState());
            }

            const DynamicAllocation gizmoAlloc = m_dynamicConstantBuffer.Allocate(gizmoCb);
            commandList->SetGraphicsRootConstantBufferView(0, gizmoAlloc.gpuAddress);
            m_gizmoMesh->Draw(commandList);
            ++totalDrawCalls;

            // Restore depth-less back buffer binding and full-screen viewport for 2D overlay
            commandList->OMSetRenderTargets(1, &backBufferRtv, FALSE, nullptr);
            commandList->RSSetViewports(1, &m_frameBuffer.GetViewport());
            commandList->RSSetScissorRects(1, &m_frameBuffer.GetScissorRect());
        }

        // Render Diagnostic Text Overlay in the top-right corner onto the post-tonemapped swap chain back buffer
        if (m_showOverlay && m_debugOverlay && m_debugOverlay->IsInitialised())
        {
            // Total scene items include renderable meshes, the camera entity, and active light entities
            const size_t cameraCount = (m_camera != nullptr) ? 1 : 0;
            const size_t lightCount  = lights.size();
            const size_t sceneItemCount = (totalSceneItems > 0) ? totalSceneItems : (m_cachedVisibleMeshCount + cameraCount + lightCount);

            Engine::OverlayStatistics stats{};
            stats.gpuName       = m_gpuName;
            stats.fps           = m_smoothedFps;
            stats.ups           = updatesPerSecond;
            stats.targetFps     = m_targetFps;
            stats.targetUps     = (targetUps > 0.0f) ? targetUps : m_targetUps;
            stats.frameTimeMs   = m_smoothedFrameTimeMs;
            stats.sampleCount   = m_frameBuffer.GetSampleCount();
            stats.objectCount   = sceneItemCount;
            stats.itemCount     = sceneItemCount;
            stats.triangleCount = m_cachedTotalTriangles;
            stats.vertexCount   = m_cachedTotalVertices;
            stats.lightCount    = static_cast<uint32_t>(lightCount);

            // Dynamic render pipeline, memory, and dispatch metrics
            stats.drawCallCount   = totalDrawCalls;
            stats.batchCount      = static_cast<uint32_t>(m_renderQueue.GetTotalBatchCount());
            stats.culledItemCount = (renderItems.size() > m_cachedVisibleMeshCount) ? (renderItems.size() - m_cachedVisibleMeshCount) : 0;
            stats.uploadUsedBytes = m_dynamicConstantBuffer.GetCurrentFrameUsedBytes();

            stats.vramLocalUsedBytes   = m_cachedVramLocalUsedBytes;
            stats.vramLocalBudgetBytes = m_cachedVramLocalBudgetBytes;
            stats.windowMode           = m_cachedWindowMode;

            // Anti-aliasing description
            if (m_frameBuffer.GetSampleCount() > 1)
            {
                stats.aaDescription = std::format("MSAA {}x", m_frameBuffer.GetSampleCount());
            }
            else
            {
                stats.aaDescription = "Off";
            }

            stats.vSync = vSync;

            // 64-bit camera spatial coordinates and atmospheric background luminance behind the overlay region
            float bgLuminance = 0.83f;
            if (m_camera)
            {
                stats.cameraPosition = m_camera->GetPosition();

                const float fovY        = m_camera->GetFovY();
                const float aspectRatio = m_camera->GetAspectRatio();
                const float tanHalfFov  = std::tan(fovY * 0.5f);

                // Top-right screen region in camera view-space ray direction
                const float camX = 0.75f * tanHalfFov * aspectRatio;
                const float camY = 0.80f * tanHalfFov;
                const Maths::Vec4 rayCamera(camX, camY, 1.0f, 0.0f);

                // Transform view ray into world space
                const Maths::Mat4x4 invView = Maths::Mat4x4(m_camera->GetViewMatrix().Inverted());
                const Maths::Vec3 worldRay  = (invView * rayCamera).ToVec3().Normalised();

                // Physical atmospheric Rayleigh scattering luminance curve:
                // Directly on the horizon rim (elevation <= 0.0): bright pale forward scattering (~0.83).
                // Above 3 degrees elevation (elevation >= 0.05): transitioning into rich blue sky (~0.45 - 0.55).
                // Approaching zenith (elevation >= 0.70): celestial deep blue (~0.38).
                const float elevation = std::max(worldRay.y, 0.0f);
                bgLuminance = std::clamp(0.38f + 0.45f * std::exp(-elevation * 18.0f), 0.35f, 1.0f);
            }
            stats.backgroundLuminance = bgLuminance;

            if (m_overlayPipelineState.GetPipelineState())
            {
                commandList->SetPipelineState(m_overlayPipelineState.GetPipelineState());
            }

            m_debugOverlay->Update(frameIndex, stats, m_width, m_height);
            m_debugOverlay->Render(commandList, frameIndex, m_width, m_height);

            // Restore primary viewport and scissor rect from frame buffer
            commandList->RSSetViewports(1, &m_frameBuffer.GetViewport());
            commandList->RSSetScissorRects(1, &m_frameBuffer.GetScissorRect());
        }

        // Transition swap chain back buffer from RENDER_TARGET to PRESENT
        D3D12_RESOURCE_BARRIER backBufferToPresent{};
        backBufferToPresent.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        backBufferToPresent.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        backBufferToPresent.Transition.pResource   = backBuffer;
        backBufferToPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        backBufferToPresent.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
        backBufferToPresent.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &backBufferToPresent);

        // Execute command list on GPU and present frame
        m_commandContext.Execute(commandQueue, frameIndex);
        m_swapChain.Present(vSync);
    }

    void Renderer::InitialiseTextureResources(ID3D12Device* device, [[maybe_unused]] ID3D12CommandQueue* commandQueue)
    {
        if (!device)
        {
            return;
        }

        // Allocate shader-visible SRV descriptor heap with headroom for offset-indexed bloom mip bindings
        constexpr UINT totalSrvDescriptors = 64;
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.NumDescriptors = totalSrvDescriptors;
        heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        heapDesc.NodeMask       = 0;

        HR_CHECK(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_srvHeap)));
        m_srvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // Allocate RTV descriptor heap for the HDR bloom mip pyramid plus luminance metering and exposure adaptation targets
        constexpr UINT autoExposureRtvCount = 2;
        D3D12_DESCRIPTOR_HEAP_DESC bloomRtvHeapDesc{};
        bloomRtvHeapDesc.NumDescriptors = BloomMipCount + autoExposureRtvCount;
        bloomRtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        bloomRtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        bloomRtvHeapDesc.NodeMask       = 0;

        HR_CHECK(device->CreateDescriptorHeap(&bloomRtvHeapDesc, IID_PPV_ARGS(&m_bloomRtvHeap)));
        m_rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        // Allocate single-entry DSV heap for the post-tonemap orientation gizmo depth buffer
        D3D12_DESCRIPTOR_HEAP_DESC gizmoDsvHeapDesc{};
        gizmoDsvHeapDesc.NumDescriptors = 1;
        gizmoDsvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        gizmoDsvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        gizmoDsvHeapDesc.NodeMask       = 0;
        HR_CHECK(device->CreateDescriptorHeap(&gizmoDsvHeapDesc, IID_PPV_ARGS(&m_gizmoDsvHeap)));

        // Initialise Cascaded Directional Shadows depth array SRV at descriptor slot 27 (register t28)
        constexpr uint32_t shadowSrvSlot = 27;
        D3D12_CPU_DESCRIPTOR_HANDLE shadowHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
        shadowHandle.ptr += shadowSrvSlot * m_srvDescriptorSize;
        m_cascadedShadowMap.CreateShaderResourceView(device, shadowHandle);

        // Initialise 8K ESA Gaia DR3 survey map at descriptor slot 31 (register t32)
        constexpr uint32_t celestialSrvSlot = 31;
        D3D12_CPU_DESCRIPTOR_HANDLE celestialHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
        celestialHandle.ptr += celestialSrvSlot * m_srvDescriptorSize;

        const std::filesystem::path skyboxPath = "resources/environment/celestial/gaia_dr3_photometry_8k.dds";
        if (std::filesystem::exists(skyboxPath) && commandQueue)
        {
            ComPtr<ID3D12CommandAllocator> uploadAlloc;
            HR_CHECK(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&uploadAlloc)));

            ComPtr<ID3D12GraphicsCommandList> uploadCmdList;
            HR_CHECK(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, uploadAlloc.Get(), nullptr, IID_PPV_ARGS(&uploadCmdList)));

            ComPtr<ID3D12Resource> staging;
            m_skyboxTexture = Texture::LoadFromDds(device, uploadCmdList.Get(), skyboxPath, staging, "GaiaDR3Photometry8K");

            if (m_skyboxTexture)
            {
                HR_CHECK(uploadCmdList->Close());
                ID3D12CommandList* const ppCmdLists[] = { uploadCmdList.Get() };
                commandQueue->ExecuteCommandLists(1, ppCmdLists);

                ComPtr<ID3D12Fence> fence;
                HR_CHECK(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
                HR_CHECK(commandQueue->Signal(fence.Get(), 1));

                HANDLE eventHandle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
                if (eventHandle)
                {
                    if (fence->GetCompletedValue() < 1)
                    {
                        HR_CHECK(fence->SetEventOnCompletion(1, eventHandle));
                        WaitForSingleObject(eventHandle, INFINITE);
                    }
                    CloseHandle(eventHandle);
                }

                m_skyboxTexture->CreateShaderResourceView(device, celestialHandle);
            }
        }

        if (!m_skyboxTexture)
        {
            // Null SRV fallback preventing unbound descriptor faults
            D3D12_SHADER_RESOURCE_VIEW_DESC nullSrvDesc{};
            nullSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            nullSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            nullSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            nullSrvDesc.TextureCube.MostDetailedMip = 0;
            nullSrvDesc.TextureCube.MipLevels = 1;
            nullSrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
            device->CreateShaderResourceView(nullptr, &nullSrvDesc, celestialHandle);
        }

        // Bind resolved HDR scene texture to descriptor slot 32 (register t33) and allocate bloom, auto-exposure, and gizmo depth targets
        UpdatePostProcessSrv(device);
        CreateBloomResources(device);
        CreateAutoExposureResources(device);
        CreateGizmoDepthBuffer(device);
    }

    void Renderer::CreateGizmoDepthBuffer(ID3D12Device* device)
    {
        if (!device || !m_gizmoDsvHeap || m_width == 0 || m_height == 0)
        {
            return;
        }

        m_gizmoDepthBuffer.Reset();

        D3D12_HEAP_PROPERTIES defaultHeapProps{};
        defaultHeapProps.Type                 = D3D12_HEAP_TYPE_DEFAULT;
        defaultHeapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        defaultHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        defaultHeapProps.CreationNodeMask     = 1;
        defaultHeapProps.VisibleNodeMask      = 1;

        D3D12_RESOURCE_DESC depthDesc{};
        depthDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthDesc.Width            = m_width;
        depthDesc.Height           = m_height;
        depthDesc.DepthOrArraySize = 1;
        depthDesc.MipLevels        = 1;
        depthDesc.Format           = DXGI_FORMAT_D32_FLOAT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depthDesc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clearValue{};
        clearValue.Format               = DXGI_FORMAT_D32_FLOAT;
        clearValue.DepthStencil.Depth   = 0.0f;
        clearValue.DepthStencil.Stencil = 0;

        HR_CHECK(device->CreateCommittedResource(
            &defaultHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &depthDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearValue,
            IID_PPV_ARGS(&m_gizmoDepthBuffer)
        ));

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
        dsvDesc.Format        = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags         = D3D12_DSV_FLAG_NONE;
        device->CreateDepthStencilView(m_gizmoDepthBuffer.Get(), &dsvDesc, m_gizmoDsvHeap->GetCPUDescriptorHandleForHeapStart());
    }

    void Renderer::CreateAutoExposureResources(ID3D12Device* device)
    {
        if (!device || !m_srvHeap || !m_bloomRtvHeap)
        {
            return;
        }

        m_luminanceMeterTexture.Reset();
        m_exposureTarget.Reset();
        m_exposureHistory.Reset();

        D3D12_HEAP_PROPERTIES defaultHeapProps{};
        defaultHeapProps.Type                 = D3D12_HEAP_TYPE_DEFAULT;
        defaultHeapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        defaultHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        defaultHeapProps.CreationNodeMask     = 1;
        defaultHeapProps.VisibleNodeMask      = 1;

        // 16x16 centre-weighted log-luminance metering target (RTV slot BloomMipCount, SRV slot 38 -> register t39)
        constexpr DXGI_FORMAT meterFormat = DXGI_FORMAT_R16G16_FLOAT;
        D3D12_RESOURCE_DESC meterDesc{};
        meterDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        meterDesc.Width              = LuminanceMeterSize;
        meterDesc.Height             = LuminanceMeterSize;
        meterDesc.DepthOrArraySize   = 1;
        meterDesc.MipLevels          = 1;
        meterDesc.Format             = meterFormat;
        meterDesc.SampleDesc.Count   = 1;
        meterDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        meterDesc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        HR_CHECK(device->CreateCommittedResource(
            &defaultHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &meterDesc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            nullptr,
            IID_PPV_ARGS(&m_luminanceMeterTexture)
        ));

        D3D12_CPU_DESCRIPTOR_HANDLE meterRtv = m_bloomRtvHeap->GetCPUDescriptorHandleForHeapStart();
        meterRtv.ptr += static_cast<SIZE_T>(BloomMipCount) * m_rtvDescriptorSize;
        D3D12_RENDER_TARGET_VIEW_DESC meterRtvDesc{};
        meterRtvDesc.Format        = meterFormat;
        meterRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        device->CreateRenderTargetView(m_luminanceMeterTexture.Get(), &meterRtvDesc, meterRtv);

        constexpr uint32_t meterSrvSlot = 38;
        D3D12_CPU_DESCRIPTOR_HANDLE meterSrv = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
        meterSrv.ptr += static_cast<SIZE_T>(meterSrvSlot) * m_srvDescriptorSize;
        D3D12_SHADER_RESOURCE_VIEW_DESC meterSrvDesc{};
        meterSrvDesc.Format                    = meterFormat;
        meterSrvDesc.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE2D;
        meterSrvDesc.Shader4ComponentMapping   = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        meterSrvDesc.Texture2D.MostDetailedMip = 0;
        meterSrvDesc.Texture2D.MipLevels       = 1;
        device->CreateShaderResourceView(m_luminanceMeterTexture.Get(), &meterSrvDesc, meterSrv);

        // 1x1 adapted exposure render target (RTV slot BloomMipCount + 1) and persistent history SRV (slot 39 -> register t40)
        constexpr DXGI_FORMAT exposureFormat = DXGI_FORMAT_R32_FLOAT;
        D3D12_RESOURCE_DESC exposureDesc{};
        exposureDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        exposureDesc.Width            = 1;
        exposureDesc.Height           = 1;
        exposureDesc.DepthOrArraySize = 1;
        exposureDesc.MipLevels        = 1;
        exposureDesc.Format           = exposureFormat;
        exposureDesc.SampleDesc.Count = 1;
        exposureDesc.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        exposureDesc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        HR_CHECK(device->CreateCommittedResource(
            &defaultHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &exposureDesc,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            nullptr,
            IID_PPV_ARGS(&m_exposureTarget)
        ));

        D3D12_CPU_DESCRIPTOR_HANDLE exposureRtv = m_bloomRtvHeap->GetCPUDescriptorHandleForHeapStart();
        exposureRtv.ptr += static_cast<SIZE_T>(BloomMipCount + 1u) * m_rtvDescriptorSize;
        D3D12_RENDER_TARGET_VIEW_DESC exposureRtvDesc{};
        exposureRtvDesc.Format        = exposureFormat;
        exposureRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        device->CreateRenderTargetView(m_exposureTarget.Get(), &exposureRtvDesc, exposureRtv);

        D3D12_RESOURCE_DESC historyDesc = exposureDesc;
        historyDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        HR_CHECK(device->CreateCommittedResource(
            &defaultHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &historyDesc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            nullptr,
            IID_PPV_ARGS(&m_exposureHistory)
        ));

        constexpr uint32_t exposureSrvSlot = 39;
        D3D12_CPU_DESCRIPTOR_HANDLE exposureSrv = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
        exposureSrv.ptr += static_cast<SIZE_T>(exposureSrvSlot) * m_srvDescriptorSize;
        D3D12_SHADER_RESOURCE_VIEW_DESC exposureSrvDesc{};
        exposureSrvDesc.Format                    = exposureFormat;
        exposureSrvDesc.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE2D;
        exposureSrvDesc.Shader4ComponentMapping   = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        exposureSrvDesc.Texture2D.MostDetailedMip = 0;
        exposureSrvDesc.Texture2D.MipLevels       = 1;
        device->CreateShaderResourceView(m_exposureHistory.Get(), &exposureSrvDesc, exposureSrv);
    }

    void Renderer::CreateBloomResources(ID3D12Device* device)
    {
        if (!device || !m_srvHeap || !m_bloomRtvHeap || m_width == 0 || m_height == 0)
        {
            return;
        }

        const DXGI_FORMAT bloomFormat = m_frameBuffer.GetRtvFormat();
        constexpr uint32_t bloomBaseSrvSlot = 33;

        uint32_t mipWidth  = std::max(m_width / 2u, 1u);
        uint32_t mipHeight = std::max(m_height / 2u, 1u);

        D3D12_HEAP_PROPERTIES defaultHeapProps{};
        defaultHeapProps.Type                 = D3D12_HEAP_TYPE_DEFAULT;
        defaultHeapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        defaultHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        defaultHeapProps.CreationNodeMask     = 1;
        defaultHeapProps.VisibleNodeMask      = 1;

        for (uint32_t mipIndex = 0; mipIndex < BloomMipCount; ++mipIndex)
        {
            BloomMip& mip = m_bloomMips[mipIndex];
            mip.texture.Reset();
            mip.width  = mipWidth;
            mip.height = mipHeight;

            D3D12_RESOURCE_DESC mipDesc{};
            mipDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            mipDesc.Alignment          = 0;
            mipDesc.Width              = mipWidth;
            mipDesc.Height             = mipHeight;
            mipDesc.DepthOrArraySize   = 1;
            mipDesc.MipLevels          = 1;
            mipDesc.Format             = bloomFormat;
            mipDesc.SampleDesc.Count   = 1;
            mipDesc.SampleDesc.Quality = 0;
            mipDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            mipDesc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

            HR_CHECK(device->CreateCommittedResource(
                &defaultHeapProps,
                D3D12_HEAP_FLAG_NONE,
                &mipDesc,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                nullptr,
                IID_PPV_ARGS(&mip.texture)
            ));

            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_bloomRtvHeap->GetCPUDescriptorHandleForHeapStart();
            rtvHandle.ptr += static_cast<SIZE_T>(mipIndex) * m_rtvDescriptorSize;

            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
            rtvDesc.Format               = bloomFormat;
            rtvDesc.ViewDimension        = D3D12_RTV_DIMENSION_TEXTURE2D;
            rtvDesc.Texture2D.MipSlice   = 0;
            rtvDesc.Texture2D.PlaneSlice = 0;
            device->CreateRenderTargetView(mip.texture.Get(), &rtvDesc, rtvHandle);

            D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
            srvHandle.ptr += static_cast<SIZE_T>(bloomBaseSrvSlot + mipIndex) * m_srvDescriptorSize;

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Format                        = bloomFormat;
            srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Texture2D.MostDetailedMip     = 0;
            srvDesc.Texture2D.MipLevels           = 1;
            srvDesc.Texture2D.PlaneSlice          = 0;
            srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
            device->CreateShaderResourceView(mip.texture.Get(), &srvDesc, srvHandle);

            mipWidth  = std::max(mipWidth / 2u, 1u);
            mipHeight = std::max(mipHeight / 2u, 1u);
        }
    }

    void Renderer::UpdatePostProcessSrv(ID3D12Device* device) noexcept
    {
        if (!device || !m_srvHeap)
        {
            return;
        }

        ID3D12Resource* hdrResource = m_frameBuffer.GetResolvedTarget();
        if (!hdrResource)
        {
            return;
        }

        constexpr uint32_t postProcessSrvSlot = 32;
        D3D12_CPU_DESCRIPTOR_HANDLE hdrSrvHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
        hdrSrvHandle.ptr += postProcessSrvSlot * m_srvDescriptorSize;

        D3D12_SHADER_RESOURCE_VIEW_DESC hdrSrvDesc{};
        hdrSrvDesc.Format                        = m_frameBuffer.GetRtvFormat();
        hdrSrvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
        hdrSrvDesc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        hdrSrvDesc.Texture2D.MostDetailedMip     = 0;
        hdrSrvDesc.Texture2D.MipLevels           = 1;
        hdrSrvDesc.Texture2D.PlaneSlice          = 0;
        hdrSrvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

        device->CreateShaderResourceView(hdrResource, &hdrSrvDesc, hdrSrvHandle);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE Renderer::GetShadowMapSrvHandle() const noexcept
    {
        if (!m_srvHeap)
        {
            return D3D12_CPU_DESCRIPTOR_HANDLE{ 0 };
        }
        constexpr uint32_t shadowSrvSlot = 27;
        D3D12_CPU_DESCRIPTOR_HANDLE handle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += shadowSrvSlot * m_srvDescriptorSize;
        return handle;
    }

    void Renderer::OnSceneInitialised() noexcept
    {
        // Emit renderer initialisation message if not already output during material staging
        if (!s_initialised)
        {
            Renderer::MarkInitialised();

            std::wcout << L"[Renderer] Renderer initialised successfully.\n";
        }
    }
}
