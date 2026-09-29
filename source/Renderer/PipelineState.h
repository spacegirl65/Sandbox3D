// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "DxCheck.h"
#include "Shader.h"

#include <d3d12.h>
#include <wrl/client.h>
#include <vector>

namespace Sandbox3D::Renderer
{
    using Microsoft::WRL::ComPtr;

    // Encapsulates Direct3D 12 Root Signature and Graphics Pipeline State Object (PSO)
    class PipelineState final
    {
    public:
        PipelineState() = default;
        ~PipelineState() = default;

        PipelineState(const PipelineState&) = delete;
        PipelineState& operator=(const PipelineState&) = delete;
        PipelineState(PipelineState&&) noexcept = default;
        PipelineState& operator=(PipelineState&&) noexcept = default;

        void Initialise(
            ID3D12Device* device,
            const Shader& vertexShader,
            const Shader& pixelShader,
            DXGI_FORMAT rtvFormat,
            DXGI_FORMAT dsvFormat = DXGI_FORMAT_D32_FLOAT,
            uint32_t sampleCount = 4,
            uint32_t quality = 0,
            bool depthWrite = true,
            D3D12_COMPARISON_FUNC depthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL
        );

        void Initialise(
            ID3D12Device* device,
            ID3D12RootSignature* rootSignature,
            const Shader& vertexShader,
            const Shader& pixelShader,
            DXGI_FORMAT rtvFormat,
            DXGI_FORMAT dsvFormat = DXGI_FORMAT_D32_FLOAT,
            uint32_t sampleCount = 4,
            uint32_t quality = 0,
            bool depthWrite = true,
            D3D12_COMPARISON_FUNC depthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL
        );

        // Initialises a dedicated depth-only pipeline state without pixel shading or colour targets
        void InitialiseDepthOnly(
            ID3D12Device* device,
            ID3D12RootSignature* rootSignature,
            const Shader& vertexShader,
            DXGI_FORMAT dsvFormat = DXGI_FORMAT_D32_FLOAT,
            uint32_t sampleCount = 4,
            uint32_t quality = 0,
            D3D12_COMPARISON_FUNC depthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL
        );

        [[nodiscard]] ID3D12RootSignature* GetRootSignature() const noexcept { return m_rootSignature.Get(); }
        [[nodiscard]] ID3D12PipelineState* GetPipelineState() const noexcept { return m_pipelineState.Get(); }

    private:
        void CreateRootSignature(ID3D12Device* device);
        void CreatePipelineState(
            ID3D12Device* device,
            const Shader& vertexShader,
            const Shader& pixelShader,
            DXGI_FORMAT rtvFormat,
            DXGI_FORMAT dsvFormat,
            uint32_t sampleCount,
            uint32_t quality,
            bool depthWrite = true,
            D3D12_COMPARISON_FUNC depthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL
        );

    private:
        ComPtr<ID3D12RootSignature> m_rootSignature;
        ComPtr<ID3D12PipelineState> m_pipelineState;
    };
}

