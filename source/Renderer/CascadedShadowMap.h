// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "DxCheck.h"
#include "PipelineState.h"
#include "DynamicUploadBuffer.h"
#include "RenderBatch.h"
#include "SceneConstantBuffer.h"
#include "Engine/Camera.h"
#include "Maths/Maths.h"

#include <d3d12.h>
#include <wrl/client.h>
#include <array>
#include <span>

namespace Sandbox3D::Renderer
{
    using Microsoft::WRL::ComPtr;

    // Manages DirectX 12 Cascaded Directional Shadows (CSM) partitioning, depth array allocation, and rasterisation passes
    class CascadedShadowMap final
    {
    public:
        static constexpr uint32_t CascadeCount = 4;
        static constexpr uint32_t ShadowMapResolution = 2048;

        CascadedShadowMap() = default;
        ~CascadedShadowMap() = default;

        CascadedShadowMap(const CascadedShadowMap&) = delete;
        CascadedShadowMap& operator=(const CascadedShadowMap&) = delete;
        CascadedShadowMap(CascadedShadowMap&&) noexcept = default;
        CascadedShadowMap& operator=(CascadedShadowMap&&) noexcept = default;

        // Allocates the Texture2DArray depth buffer, DSV slices, and shadow pipeline state
        void Initialise(ID3D12Device* device, ID3D12RootSignature* rootSignature);

        // Releases all allocated Direct3D 12 shadow resources and descriptors
        void Shutdown() noexcept;

        // Binds the Texture2DArray Shader Resource View to the specified descriptor heap handle at root slot t44
        void CreateShaderResourceView(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE destHandle);

        // Partitions camera view frustum into logarithmic depth splits and snaps orthographic projections to shadow texels
        void UpdateCascades(const Engine::Camera& camera, const Maths::Vec3& sunDirection);

        // Executes the shadow depth pass by clearing slices and rasterising terrain and scene entities into each cascade
        void ExecuteShadowPass(
            ID3D12GraphicsCommandList* commandList,
            std::span<const RenderBatch> opaqueBatches,
            const Engine::Camera* camera,
            DynamicUploadBuffer& dynamicUploadBuffer,
            const SceneConstantBuffer& baseSceneCb
        );

        [[nodiscard]] const std::array<Maths::Mat4x4, CascadeCount>& GetLightViewProjMatrices() const noexcept { return m_lightViewProj; }
        [[nodiscard]] const Maths::Mat4x4& GetLightViewProj(size_t index) const noexcept { return m_lightViewProj[index]; }
        [[nodiscard]] const Maths::Vec4& GetCascadeSplits() const noexcept { return m_cascadeSplits; }
        [[nodiscard]] const Maths::Vec4& GetShadowParams() const noexcept { return m_shadowParams; }
        [[nodiscard]] ID3D12Resource* GetResource() const noexcept { return m_shadowDepthArray.Get(); }
        [[nodiscard]] bool IsInitialised() const noexcept { return m_isInitialised; }

    private:
        ComPtr<ID3D12Resource>               m_shadowDepthArray;
        ComPtr<ID3D12DescriptorHeap>         m_dsvHeap;
        uint32_t                             m_dsvDescriptorSize{ 0 };
        std::array<D3D12_CPU_DESCRIPTOR_HANDLE, CascadeCount> m_dsvHandles{};

        D3D12_VIEWPORT                       m_viewport{};
        D3D12_RECT                           m_scissorRect{};

        PipelineState                        m_shadowPipelineState;

        std::array<Maths::Mat4x4, CascadeCount> m_lightViewProj{};
        Maths::Vec4                          m_cascadeSplits{ 25.0f, 100.0f, 400.0f, 1600.0f };
        Maths::Vec4                          m_shadowParams{
            static_cast<float>(ShadowMapResolution),
            1.0f / static_cast<float>(ShadowMapResolution),
            1.0f,
            static_cast<float>(CascadeCount)
        };

        struct CascadeBounds
        {
            float minX{ 0.0f };
            float maxX{ 0.0f };
            float minY{ 0.0f };
            float maxY{ 0.0f };
            float minZ{ 0.0f };
            float maxZ{ 0.0f };
        };

        std::array<CascadeBounds, CascadeCount> m_cascadeBounds{};
        Maths::Vec3                          m_lightDir{ 0.0f, -1.0f, 0.0f };
        Maths::Vec3                          m_lightRight{ 1.0f, 0.0f, 0.0f };
        Maths::Vec3                          m_lightUp{ 0.0f, 0.0f, 1.0f };

        D3D12_RESOURCE_STATES                m_currentState{ D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
        bool                                 m_isInitialised{ false };
    };
}

