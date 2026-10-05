// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "CascadedShadowMap.h"
#include "Shader.h"
#include "Mesh.h"
#include "Shaders.h"

#include <cmath>
#include <filesystem>
#include <algorithm>
#include <string>

namespace Sandbox3D::Renderer
{
    void CascadedShadowMap::Initialise(ID3D12Device* device, ID3D12RootSignature* rootSignature)
    {
        if (!device || !rootSignature || m_isInitialised)
        {
            return;
        }

        // Compile depth-only vertex shader stage for fast shadow rasterisation
        Shader depthPassVs;
        const std::filesystem::path shaderPath = "source/Shaders/DepthOnlyPass.hlsl";
        if (std::filesystem::exists(shaderPath))
        {
            depthPassVs.CompileFromFile(shaderPath, "VSMain", ShaderStage::Vertex);
        }
        else
        {
            const std::string shadowSource = std::string(s_embeddedSceneBuffers) + s_embeddedDepthOnlyPassVertexStage;
            depthPassVs.CompileFromSource(shadowSource, "EmbeddedDepthOnlyPass.hlsl", "VSMain", ShaderStage::Vertex);
        }

        // Construct lightweight depth-only Pipeline State Object with slope-scaled depth bias
        constexpr int shadowDepthBias = 10;
        constexpr float shadowSlopeBias = 1.0f;
        m_shadowPipelineState.InitialiseShadowDepth(
            device,
            rootSignature,
            depthPassVs,
            DXGI_FORMAT_D32_FLOAT,
            D3D12_CULL_MODE_BACK,
            shadowDepthBias,
            shadowSlopeBias
        );

        // Allocate single Direct3D 12 depth-stencil resource Texture2DArray (2048 x 2048 x 4 slices)
        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type                 = D3D12_HEAP_TYPE_DEFAULT;
        heapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask     = 0;
        heapProps.VisibleNodeMask      = 0;

        D3D12_RESOURCE_DESC texDesc{};
        texDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Alignment          = 0;
        texDesc.Width              = ShadowMapResolution;
        texDesc.Height             = ShadowMapResolution;
        texDesc.DepthOrArraySize   = static_cast<UINT16>(CascadeCount);
        texDesc.MipLevels          = 1;
        texDesc.Format             = DXGI_FORMAT_R32_TYPELESS;
        texDesc.SampleDesc.Count   = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        texDesc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clearValue{};
        clearValue.Format               = DXGI_FORMAT_D32_FLOAT;
        clearValue.DepthStencil.Depth   = 1.0f;
        clearValue.DepthStencil.Stencil = 0;

        HR_CHECK(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &clearValue,
            IID_PPV_ARGS(&m_shadowDepthArray)
        ));
        m_shadowDepthArray->SetName(L"CascadedShadowMapDepthArray");
        m_currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        // Allocate dedicated Depth Stencil View descriptor heap for array slices
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
        dsvHeapDesc.NumDescriptors = CascadeCount;
        dsvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        dsvHeapDesc.NodeMask       = 0;

        HR_CHECK(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));
        m_dsvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
        for (uint32_t i = 0; i < CascadeCount; ++i)
        {
            D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
            dsvDesc.Format                         = DXGI_FORMAT_D32_FLOAT;
            dsvDesc.ViewDimension                  = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
            dsvDesc.Flags                          = D3D12_DSV_FLAG_NONE;
            dsvDesc.Texture2DArray.MipSlice        = 0;
            dsvDesc.Texture2DArray.FirstArraySlice = i;
            dsvDesc.Texture2DArray.ArraySize       = 1;

            m_dsvHandles[i] = dsvHandle;
            device->CreateDepthStencilView(m_shadowDepthArray.Get(), &dsvDesc, dsvHandle);
            dsvHandle.ptr += m_dsvDescriptorSize;
        }

        // Configure viewport and scissor rect matching discrete shadow map resolution
        m_viewport.TopLeftX = 0.0f;
        m_viewport.TopLeftY = 0.0f;
        m_viewport.Width    = static_cast<float>(ShadowMapResolution);
        m_viewport.Height   = static_cast<float>(ShadowMapResolution);
        m_viewport.MinDepth = 0.0f;
        m_viewport.MaxDepth = 1.0f;

        m_scissorRect.left   = 0;
        m_scissorRect.top    = 0;
        m_scissorRect.right  = static_cast<LONG>(ShadowMapResolution);
        m_scissorRect.bottom = static_cast<LONG>(ShadowMapResolution);

        m_isInitialised = true;
    }

    void CascadedShadowMap::Shutdown() noexcept
    {
        m_shadowDepthArray.Reset();
        m_dsvHeap.Reset();
        m_shadowPipelineState = {};
        m_isInitialised = false;
    }

    void CascadedShadowMap::CreateShaderResourceView(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE destHandle)
    {
        if (!device || !m_shadowDepthArray)
        {
            return;
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format                            = DXGI_FORMAT_R32_FLOAT;
        srvDesc.ViewDimension                     = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Shader4ComponentMapping           = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2DArray.MostDetailedMip    = 0;
        srvDesc.Texture2DArray.MipLevels          = 1;
        srvDesc.Texture2DArray.FirstArraySlice    = 0;
        srvDesc.Texture2DArray.ArraySize          = CascadeCount;
        srvDesc.Texture2DArray.PlaneSlice         = 0;
        srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;

        device->CreateShaderResourceView(m_shadowDepthArray.Get(), &srvDesc, destHandle);
    }

    void CascadedShadowMap::UpdateCascades(const Engine::Camera& camera, const Maths::Vec3& sunDirection)
    {
        const float fovY = camera.GetFovY();
        const float aspectRatio = camera.GetAspectRatio();
        const float tanHalfFov = std::tan(fovY * 0.5f);

        const Maths::Mat4x4D& viewMatrix = camera.GetViewMatrix();
        // Camera view basis vectors in world space (columns of view transformation)
        const Maths::Vec3 camRight(
            static_cast<float>(viewMatrix.m[0][0]),
            static_cast<float>(viewMatrix.m[1][0]),
            static_cast<float>(viewMatrix.m[2][0])
        );
        const Maths::Vec3 camUp(
            static_cast<float>(viewMatrix.m[0][1]),
            static_cast<float>(viewMatrix.m[1][1]),
            static_cast<float>(viewMatrix.m[2][1])
        );
        const Maths::Vec3 camForward(
            static_cast<float>(viewMatrix.m[0][2]),
            static_cast<float>(viewMatrix.m[1][2]),
            static_cast<float>(viewMatrix.m[2][2])
        );

        // Normalize solar light propagation direction vector
        Maths::Vec3 lightDir = sunDirection.Normalised();
        if (lightDir.LengthSquared() < 0.0001f)
        {
            lightDir = Maths::Vec3(-0.35f, -0.92f, -0.18f).Normalised();
        }

        // Establish light coordinate frame oriented along the sun's directional vector
        const Maths::Vec3 upFallback = (std::abs(lightDir.y) > 0.99f)
            ? Maths::Vec3(0.0f, 0.0f, 1.0f)
            : Maths::Vec3(0.0f, 1.0f, 0.0f);
        m_lightRight = upFallback.Cross(lightDir).Normalised();
        m_lightUp    = lightDir.Cross(m_lightRight).Normalised();
        m_lightDir   = lightDir;

        // Light view orientation matrix (row-vector convention)
        const Maths::Mat4x4 lightView(
            m_lightRight.x, m_lightUp.x, m_lightDir.x, 0.0f,
            m_lightRight.y, m_lightUp.y, m_lightDir.y, 0.0f,
            m_lightRight.z, m_lightUp.z, m_lightDir.z, 0.0f,
            0.0f,           0.0f,         0.0f,          1.0f
        );

        // Logarithmic cascade depth partition split intervals (0-25m, 25-100m, 100-400m, 400-1600m)
        constexpr float cascadeNearDepths[CascadeCount] = { 0.1f,   25.0f,  100.0f,  400.0f };
        constexpr float cascadeFarDepths[CascadeCount]  = { 25.0f, 100.0f,  400.0f, 1600.0f };

        m_cascadeSplits = Maths::Vec4(
            cascadeFarDepths[0],
            cascadeFarDepths[1],
            cascadeFarDepths[2],
            cascadeFarDepths[3]
        );

        const float shadowRes = static_cast<float>(ShadowMapResolution);

        for (uint32_t c = 0; c < CascadeCount; ++c)
        {
            const float nearZ = cascadeNearDepths[c];
            const float farZ  = cascadeFarDepths[c];

            const float nearH = nearZ * tanHalfFov;
            const float nearW = nearH * aspectRatio;
            const float farH  = farZ * tanHalfFov;
            const float farW  = farH * aspectRatio;

            // Eight camera-relative view frustum corner positions
            const Maths::Vec3 frustumCornersView[8] = {
                Maths::Vec3(-nearW, -nearH, nearZ),
                Maths::Vec3( nearW, -nearH, nearZ),
                Maths::Vec3(-nearW,  nearH, nearZ),
                Maths::Vec3( nearW,  nearH, nearZ),
                Maths::Vec3(-farW,  -farH,  farZ),
                Maths::Vec3( farW,  -farH,  farZ),
                Maths::Vec3(-farW,   farH,  farZ),
                Maths::Vec3( farW,   farH,  farZ)
            };

            float minX =  1e9f;
            float maxX = -1e9f;
            float minY =  1e9f;
            float maxY = -1e9f;
            float minZ =  1e9f;
            float maxZ = -1e9f;

            for (const auto& cornerView : frustumCornersView)
            {
                // Transform to camera-relative world space
                const Maths::Vec3 cornerWorld =
                    camRight * cornerView.x +
                    camUp * cornerView.y +
                    camForward * cornerView.z;

                // Transform to light view space
                const float lx = cornerWorld.Dot(m_lightRight);
                const float ly = cornerWorld.Dot(m_lightUp);
                const float lz = cornerWorld.Dot(m_lightDir);

                minX = std::min(minX, lx);
                maxX = std::max(maxX, lx);
                minY = std::min(minY, ly);
                maxY = std::max(maxY, ly);
                minZ = std::min(minZ, lz);
                maxZ = std::max(maxZ, lz);
            }

            // Snap orthographic projection boundaries to discrete shadow texel units to eliminate edge shimmering
            const float extentX = maxX - minX;
            const float extentY = maxY - minY;
            const float maxExtent = std::max(extentX, extentY);
            const float texelSize = maxExtent / shadowRes;

            minX = std::floor(minX / texelSize) * texelSize;
            maxX = minX + std::ceil(extentX / texelSize) * texelSize;
            minY = std::floor(minY / texelSize) * texelSize;
            maxY = minY + std::ceil(extentY / texelSize) * texelSize;

            // Extend minZ backwards along the light ray to capture occluders scaled to the cascade partition footprint
            constexpr float cascadeCasterExtensions[CascadeCount] = { 80.0f, 250.0f, 800.0f, 2000.0f };
            minZ -= cascadeCasterExtensions[c];
            maxZ += 50.0f;

            m_cascadeBounds[c] = CascadeBounds{ minX, maxX, minY, maxY, minZ, maxZ };

            // Construct orthographic projection matrix (row-vector convention: [x, y, z, 1] * M)
            const float invWidth  = 1.0f / (maxX - minX);
            const float invHeight = 1.0f / (maxY - minY);
            const float invDepth  = 1.0f / (maxZ - minZ);

            const Maths::Mat4x4 orthoProj(
                2.0f * invWidth,           0.0f,                      0.0f,             0.0f,
                0.0f,                      2.0f * invHeight,          0.0f,             0.0f,
                0.0f,                      0.0f,                      invDepth,         0.0f,
                -(maxX + minX) * invWidth, -(maxY + minY) * invHeight, -minZ * invDepth, 1.0f
            );

            m_lightViewProj[c] = lightView * orthoProj;
        }
    }

    void CascadedShadowMap::ExecuteShadowPass(
        ID3D12GraphicsCommandList* commandList,
        std::span<const RenderBatch> opaqueBatches,
        const Engine::Camera* camera,
        DynamicUploadBuffer& dynamicUploadBuffer,
        const SceneConstantBuffer& baseSceneCb
    )
    {
        if (!commandList || !m_isInitialised)
        {
            return;
        }

        // Transition shadow depth array resource from PIXEL_SHADER_RESOURCE to DEPTH_WRITE
        if (m_currentState != D3D12_RESOURCE_STATE_DEPTH_WRITE)
        {
            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource   = m_shadowDepthArray.Get();
            barrier.Transition.StateBefore = m_currentState;
            barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_DEPTH_WRITE;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            commandList->ResourceBarrier(1, &barrier);
            m_currentState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        }

        commandList->SetPipelineState(m_shadowPipelineState.GetPipelineState());
        commandList->RSSetViewports(1, &m_viewport);
        commandList->RSSetScissorRects(1, &m_scissorRect);

        // Render terrain mesh and active scene entities into each cascade slice
        for (uint32_t cascadeIdx = 0; cascadeIdx < CascadeCount; ++cascadeIdx)
        {
            const D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_dsvHandles[cascadeIdx];
            commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
            commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsv);

            const Maths::Mat4x4& lightViewProj = m_lightViewProj[cascadeIdx];

            const Mesh* boundMesh = nullptr;
            for (const auto& batch : opaqueBatches)
            {
                if (batch.items.empty() || !batch.mesh)
                {
                    continue;
                }

                // Bypass unlit wireframe and debug visualiser batches from casting directional shadows
                if (batch.material && batch.material->IsUnlit())
                {
                    continue;
                }

                if (batch.mesh != boundMesh)
                {
                    boundMesh = batch.mesh;
                    boundMesh->Bind(commandList);
                }

                const size_t instanceCount = batch.items.size();
                const bool canUseDirectPath = (instanceCount == 1 && batch.items[0]->colorTint == Maths::Vec4::One());

                if (canUseDirectPath)
                {
                    const auto* item = batch.items[0];
                    const Maths::Mat4x4 camWorld = camera
                        ? camera->CalculateCameraRelativeWorld(item->worldMatrix)
                        : Maths::Mat4x4(item->worldMatrix);

                    // Perform fast light-space sphere culling against the cascade bounding box
                    const auto& sphere = boundMesh->GetBoundingSphere();
                    if (sphere.radius > 0.0f)
                    {
                        const Maths::Vec3 centerCamRel = camWorld.TransformPoint(sphere.center);
                        const float lx = centerCamRel.Dot(m_lightRight);
                        const float ly = centerCamRel.Dot(m_lightUp);
                        const float lz = centerCamRel.Dot(m_lightDir);
                        const float r  = sphere.radius;
                        const auto& bounds = m_cascadeBounds[cascadeIdx];

                        if (lx + r < bounds.minX || lx - r > bounds.maxX ||
                            ly + r < bounds.minY || ly - r > bounds.maxY ||
                            lz + r < bounds.minZ || lz - r > bounds.maxZ)
                        {
                            continue;
                        }
                    }

                    SceneConstantBuffer cbData = baseSceneCb;
                    cbData.mvp         = camWorld * lightViewProj;
                    cbData.world       = camWorld;
                    cbData.isInstanced = 0;

                    const DynamicAllocation cbAlloc = dynamicUploadBuffer.Allocate(cbData);
                    commandList->SetGraphicsRootConstantBufferView(0, cbAlloc.gpuAddress);

                    boundMesh->DrawBound(commandList, 1, 0);
                }
                else
                {
                    SceneConstantBuffer cbData = baseSceneCb;
                    cbData.mvp         = Maths::Mat4x4::Identity();
                    cbData.world       = Maths::Mat4x4::Identity();
                    cbData.isInstanced = 1;

                    const DynamicAllocation cbAlloc = dynamicUploadBuffer.Allocate(cbData);
                    commandList->SetGraphicsRootConstantBufferView(0, cbAlloc.gpuAddress);

                    const size_t byteSize = instanceCount * sizeof(GpuInstanceData);
                    const DynamicAllocation instAlloc = dynamicUploadBuffer.Allocate(byteSize, 16);
                    auto* dest = static_cast<GpuInstanceData*>(instAlloc.cpuAddress);

                    size_t visibleInstanceCount = 0;
                    const auto& bounds = m_cascadeBounds[cascadeIdx];
                    const auto& sphere = boundMesh->GetBoundingSphere();

                    for (size_t i = 0; i < instanceCount; ++i)
                    {
                        const auto* item = batch.items[i];
                        const Maths::Mat4x4 camWorld = camera
                            ? camera->CalculateCameraRelativeWorld(item->worldMatrix)
                            : Maths::Mat4x4(item->worldMatrix);

                        if (sphere.radius > 0.0f)
                        {
                            const Maths::Vec3 centerCamRel = camWorld.TransformPoint(sphere.center);
                            const float lx = centerCamRel.Dot(m_lightRight);
                            const float ly = centerCamRel.Dot(m_lightUp);
                            const float lz = centerCamRel.Dot(m_lightDir);
                            const float r  = sphere.radius;

                            if (lx + r < bounds.minX || lx - r > bounds.maxX ||
                                ly + r < bounds.minY || ly - r > bounds.maxY ||
                                lz + r < bounds.minZ || lz - r > bounds.maxZ)
                            {
                                continue;
                            }
                        }

                        dest[visibleInstanceCount].cameraRelativeMVP   = camWorld * lightViewProj;
                        dest[visibleInstanceCount].cameraRelativeWorld = camWorld;
                        dest[visibleInstanceCount].colorTint           = item->colorTint;
                        ++visibleInstanceCount;
                    }

                    if (visibleInstanceCount > 0)
                    {
                        commandList->SetGraphicsRootShaderResourceView(1, instAlloc.gpuAddress);
                        boundMesh->DrawBound(commandList, static_cast<uint32_t>(visibleInstanceCount), 0);
                    }
                }
            }
        }

        // Transition shadow depth array resource to PIXEL_SHADER_RESOURCE for receiver shading
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource   = m_shadowDepthArray.Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);
        m_currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
}
