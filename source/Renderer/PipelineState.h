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
            DXGI_FORMAT rtvFormat
        );

        [[nodiscard]] ID3D12RootSignature* GetRootSignature() const noexcept { return m_rootSignature.Get(); }
        [[nodiscard]] ID3D12PipelineState* GetPipelineState() const noexcept { return m_pipelineState.Get(); }

    private:
        void CreateRootSignature(ID3D12Device* device);
        void CreatePipelineState(
            ID3D12Device* device,
            const Shader& vertexShader,
            const Shader& pixelShader,
            DXGI_FORMAT rtvFormat
        );

    private:
        ComPtr<ID3D12RootSignature> m_rootSignature;
        ComPtr<ID3D12PipelineState> m_pipelineState;
    };
}

