// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "PipelineState.h"

#include <stdexcept>

namespace Sandbox3D::Renderer
{
    void PipelineState::Initialise(
        ID3D12Device* device,
        const Shader& vertexShader,
        const Shader& pixelShader,
        DXGI_FORMAT rtvFormat,
        DXGI_FORMAT dsvFormat
    )
    {
        CreateRootSignature(device);
        CreatePipelineState(device, vertexShader, pixelShader, rtvFormat, dsvFormat);
    }

    void PipelineState::CreateRootSignature(ID3D12Device* device)
    {
        // Root parameter 0: Root CBV at register b0 (SceneConstantBuffer)
        D3D12_ROOT_PARAMETER rootParameter = {};
        rootParameter.ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameter.Descriptor.ShaderRegister = 0;
        rootParameter.Descriptor.RegisterSpace  = 0;
        rootParameter.ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
        rootSignatureDesc.NumParameters = 1;
        rootSignatureDesc.pParameters   = &rootParameter;
        rootSignatureDesc.NumStaticSamplers = 0;
        rootSignatureDesc.pStaticSamplers   = nullptr;
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> signatureBlob;
        ComPtr<ID3DBlob> errorBlob;

        const HRESULT hr = D3D12SerializeRootSignature(
            &rootSignatureDesc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            &signatureBlob,
            &errorBlob
        );

        if (FAILED(hr))
        {
            std::string err = errorBlob ? static_cast<const char*>(errorBlob->GetBufferPointer()) : "Root signature serialization failed.";
            throw std::runtime_error(err);
        }

        HR_CHECK(device->CreateRootSignature(
            0,
            signatureBlob->GetBufferPointer(),
            signatureBlob->GetBufferSize(),
            IID_PPV_ARGS(&m_rootSignature)
        ));
    }

    void PipelineState::CreatePipelineState(
        ID3D12Device* device,
        const Shader& vertexShader,
        const Shader& pixelShader,
        DXGI_FORMAT rtvFormat,
        DXGI_FORMAT dsvFormat
    )
    {
        // Define vertex input layout
        constexpr D3D12_INPUT_ELEMENT_DESC inputElements[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,                            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // Configure rasteriser state
        D3D12_RASTERIZER_DESC rasterizerDesc = {};
        rasterizerDesc.FillMode              = D3D12_FILL_MODE_SOLID;
        rasterizerDesc.CullMode              = D3D12_CULL_MODE_BACK; // Cull back-facing triangles to eliminate overdraw
        rasterizerDesc.FrontCounterClockwise = FALSE;
        rasterizerDesc.DepthBias             = D3D12_DEFAULT_DEPTH_BIAS;
        rasterizerDesc.DepthBiasClamp        = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        rasterizerDesc.SlopeScaledDepthBias  = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rasterizerDesc.DepthClipEnable       = TRUE;
        rasterizerDesc.MultisampleEnable     = FALSE;
        rasterizerDesc.AntialiasedLineEnable = FALSE;
        rasterizerDesc.ForcedSampleCount     = 0;
        rasterizerDesc.ConservativeRaster    = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        // Configure blend state
        D3D12_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable  = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;

        const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {
            FALSE, FALSE,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL,
        };

        for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        {
            blendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;
        }

        // Configure depth-stencil state
        D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
        if (dsvFormat != DXGI_FORMAT_UNKNOWN)
        {
            depthStencilDesc.DepthEnable    = TRUE;
            depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
            depthStencilDesc.DepthFunc      = D3D12_COMPARISON_FUNC_LESS;
            depthStencilDesc.StencilEnable  = FALSE;
        }

        // Configure graphics pipeline state description
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature        = m_rootSignature.Get();
        psoDesc.VS                    = vertexShader.GetBytecode();
        psoDesc.PS                    = pixelShader.GetBytecode();
        psoDesc.BlendState            = blendDesc;
        psoDesc.SampleMask            = UINT_MAX;
        psoDesc.RasterizerState       = rasterizerDesc;
        psoDesc.DepthStencilState     = depthStencilDesc;
        psoDesc.DSVFormat             = dsvFormat;
        psoDesc.InputLayout           = { inputElements, _countof(inputElements) };
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets      = 1;
        psoDesc.RTVFormats[0]         = rtvFormat;
        psoDesc.SampleDesc.Count      = 1;
        psoDesc.SampleDesc.Quality    = 0;

        HR_CHECK(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
    }
}

