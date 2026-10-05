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
        DXGI_FORMAT dsvFormat,
        uint32_t sampleCount,
        uint32_t quality,
        bool depthWrite,
        D3D12_COMPARISON_FUNC depthFunc,
        D3D12_CULL_MODE cullMode
    )
    {
        CreateRootSignature(device);
        CreatePipelineState(device, vertexShader, pixelShader, rtvFormat, dsvFormat, sampleCount, quality, depthWrite, depthFunc, cullMode);
    }

    void PipelineState::Initialise(
        ID3D12Device* device,
        ID3D12RootSignature* rootSignature,
        const Shader& vertexShader,
        const Shader& pixelShader,
        DXGI_FORMAT rtvFormat,
        DXGI_FORMAT dsvFormat,
        uint32_t sampleCount,
        uint32_t quality,
        bool depthWrite,
        D3D12_COMPARISON_FUNC depthFunc,
        D3D12_CULL_MODE cullMode
    )
    {
        if (rootSignature)
        {
            m_rootSignature = rootSignature;
        }
        else
        {
            CreateRootSignature(device);
        }
        CreatePipelineState(device, vertexShader, pixelShader, rtvFormat, dsvFormat, sampleCount, quality, depthWrite, depthFunc, cullMode);
    }

    void PipelineState::InitialiseDepthOnly(
        ID3D12Device* device,
        ID3D12RootSignature* rootSignature,
        const Shader& vertexShader,
        DXGI_FORMAT dsvFormat,
        uint32_t sampleCount,
        uint32_t quality,
        D3D12_COMPARISON_FUNC depthFunc
    )
    {
        if (rootSignature)
        {
            m_rootSignature = rootSignature;
        }
        else
        {
            CreateRootSignature(device);
        }

        // Define vertex input layout
        constexpr D3D12_INPUT_ELEMENT_DESC inputElements[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,                            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // Configure rasteriser state: backface culling enabled for depth-only rendering
        D3D12_RASTERIZER_DESC rasterizerDesc = {};
        rasterizerDesc.FillMode              = D3D12_FILL_MODE_SOLID;
        rasterizerDesc.CullMode              = D3D12_CULL_MODE_BACK;
        rasterizerDesc.FrontCounterClockwise = FALSE;
        rasterizerDesc.DepthBias             = D3D12_DEFAULT_DEPTH_BIAS;
        rasterizerDesc.DepthBiasClamp        = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        rasterizerDesc.SlopeScaledDepthBias  = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rasterizerDesc.DepthClipEnable       = TRUE;
        rasterizerDesc.MultisampleEnable     = (sampleCount > 1) ? TRUE : FALSE;
        rasterizerDesc.AntialiasedLineEnable = FALSE;
        rasterizerDesc.ForcedSampleCount     = 0;
        rasterizerDesc.ConservativeRaster    = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        // Depth-only rendering writes depth with no colour targets
        D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
        depthStencilDesc.DepthEnable    = TRUE;
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        depthStencilDesc.DepthFunc      = depthFunc;
        depthStencilDesc.StencilEnable  = FALSE;

        D3D12_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable  = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature        = m_rootSignature.Get();
        psoDesc.VS                    = vertexShader.GetBytecode();
        psoDesc.PS                    = { nullptr, 0 };
        psoDesc.BlendState            = blendDesc;
        psoDesc.SampleMask            = UINT_MAX;
        psoDesc.RasterizerState       = rasterizerDesc;
        psoDesc.DepthStencilState     = depthStencilDesc;
        psoDesc.DSVFormat             = dsvFormat;
        psoDesc.InputLayout           = { inputElements, _countof(inputElements) };
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets      = 0;
        psoDesc.SampleDesc.Count      = sampleCount;
        psoDesc.SampleDesc.Quality    = quality;

        HR_CHECK(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
    }

    void PipelineState::InitialiseShadowDepth(
        ID3D12Device* device,
        ID3D12RootSignature* rootSignature,
        const Shader& vertexShader,
        DXGI_FORMAT dsvFormat,
        D3D12_CULL_MODE cullMode,
        int depthBias,
        float slopeScaledDepthBias
    )
    {
        if (rootSignature)
        {
            m_rootSignature = rootSignature;
        }
        else
        {
            CreateRootSignature(device);
        }

        // Define vertex input layout matching engine mesh standard
        constexpr D3D12_INPUT_ELEMENT_DESC inputElements[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,                            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // Rasteriser configuration: hardware slope-scaled depth bias for self-shadow acne prevention
        D3D12_RASTERIZER_DESC rasterizerDesc = {};
        rasterizerDesc.FillMode              = D3D12_FILL_MODE_SOLID;
        rasterizerDesc.CullMode              = cullMode;
        rasterizerDesc.FrontCounterClockwise = FALSE;
        rasterizerDesc.DepthBias             = depthBias;
        rasterizerDesc.DepthBiasClamp        = 0.0f;
        rasterizerDesc.SlopeScaledDepthBias  = slopeScaledDepthBias;
        rasterizerDesc.DepthClipEnable       = TRUE;
        rasterizerDesc.MultisampleEnable     = FALSE;
        rasterizerDesc.AntialiasedLineEnable = FALSE;
        rasterizerDesc.ForcedSampleCount     = 0;
        rasterizerDesc.ConservativeRaster    = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        // Shadow mapping writes depth with standard less-equal depth testing and no colour targets
        D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
        depthStencilDesc.DepthEnable    = TRUE;
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        depthStencilDesc.DepthFunc      = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        depthStencilDesc.StencilEnable  = FALSE;

        D3D12_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable  = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature        = m_rootSignature.Get();
        psoDesc.VS                    = vertexShader.GetBytecode();
        psoDesc.PS                    = { nullptr, 0 };
        psoDesc.BlendState            = blendDesc;
        psoDesc.SampleMask            = UINT_MAX;
        psoDesc.RasterizerState       = rasterizerDesc;
        psoDesc.DepthStencilState     = depthStencilDesc;
        psoDesc.DSVFormat             = dsvFormat;
        psoDesc.InputLayout           = { inputElements, _countof(inputElements) };
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets      = 0;
        psoDesc.SampleDesc.Count      = 1;
        psoDesc.SampleDesc.Quality    = 0;

        HR_CHECK(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
    }

    void PipelineState::CreateRootSignature(ID3D12Device* device)
    {
        // Descriptor table range covering texture shader resource views
        D3D12_DESCRIPTOR_RANGE descriptorRange{};
        descriptorRange.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        descriptorRange.NumDescriptors                    = 48;
        descriptorRange.BaseShaderRegister                = 1;
        descriptorRange.RegisterSpace                     = 0;
        descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        // Root parameters: constant buffer view, structured instance buffer SRV, texture descriptor table, and shader-specific 32-bit constants
        D3D12_ROOT_PARAMETER rootParameters[4] = {};
        rootParameters[0].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[0].Descriptor.ShaderRegister = 0;
        rootParameters[0].Descriptor.RegisterSpace  = 0;
        rootParameters[0].ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;

        rootParameters[1].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_SRV;
        rootParameters[1].Descriptor.ShaderRegister = 0;
        rootParameters[1].Descriptor.RegisterSpace  = 0;
        rootParameters[1].ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;

        rootParameters[2].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
        rootParameters[2].DescriptorTable.pDescriptorRanges   = &descriptorRange;
        rootParameters[2].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;

        rootParameters[3].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParameters[3].Constants.ShaderRegister  = 1;
        rootParameters[3].Constants.RegisterSpace   = 0;
        rootParameters[3].Constants.Num32BitValues  = 4;
        rootParameters[3].ShaderVisibility          = D3D12_SHADER_VISIBILITY_PIXEL;

        // Static samplers: anisotropic 16x wrap sampler in s0, bilinear clamp sampler in s1, and comparison linear clamp sampler in s2
        D3D12_STATIC_SAMPLER_DESC staticSamplers[3] = {};
        staticSamplers[0].Filter           = D3D12_FILTER_ANISOTROPIC;
        staticSamplers[0].AddressU         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSamplers[0].AddressV         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSamplers[0].AddressW         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSamplers[0].MipLODBias       = 0.0f;
        staticSamplers[0].MaxAnisotropy    = 16;
        staticSamplers[0].ComparisonFunc   = D3D12_COMPARISON_FUNC_ALWAYS;
        staticSamplers[0].BorderColor      = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        staticSamplers[0].MinLOD           = 0.0f;
        staticSamplers[0].MaxLOD           = D3D12_FLOAT32_MAX;
        staticSamplers[0].ShaderRegister   = 0;
        staticSamplers[0].RegisterSpace    = 0;
        staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        staticSamplers[1].Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        staticSamplers[1].AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSamplers[1].AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSamplers[1].AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSamplers[1].MipLODBias       = 0.0f;
        staticSamplers[1].MaxAnisotropy    = 1;
        staticSamplers[1].ComparisonFunc   = D3D12_COMPARISON_FUNC_ALWAYS;
        staticSamplers[1].BorderColor      = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        staticSamplers[1].MinLOD           = 0.0f;
        staticSamplers[1].MaxLOD           = D3D12_FLOAT32_MAX;
        staticSamplers[1].ShaderRegister   = 1;
        staticSamplers[1].RegisterSpace    = 0;
        staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        staticSamplers[2].Filter           = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        staticSamplers[2].AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSamplers[2].AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSamplers[2].AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSamplers[2].MipLODBias       = 0.0f;
        staticSamplers[2].MaxAnisotropy    = 1;
        staticSamplers[2].ComparisonFunc   = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        staticSamplers[2].BorderColor      = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
        staticSamplers[2].MinLOD           = 0.0f;
        staticSamplers[2].MaxLOD           = D3D12_FLOAT32_MAX;
        staticSamplers[2].ShaderRegister   = 2;
        staticSamplers[2].RegisterSpace    = 0;
        staticSamplers[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
        rootSignatureDesc.NumParameters     = 4;
        rootSignatureDesc.pParameters       = rootParameters;
        rootSignatureDesc.NumStaticSamplers = 3;
        rootSignatureDesc.pStaticSamplers   = staticSamplers;
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
        DXGI_FORMAT dsvFormat,
        uint32_t sampleCount,
        uint32_t quality,
        bool depthWrite,
        D3D12_COMPARISON_FUNC depthFunc,
        D3D12_CULL_MODE cullMode
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
        rasterizerDesc.CullMode              = cullMode; // Cull back-facing triangles to eliminate overdraw, or NONE for sky/quad
        rasterizerDesc.FrontCounterClockwise = FALSE;
        rasterizerDesc.DepthBias             = D3D12_DEFAULT_DEPTH_BIAS;
        rasterizerDesc.DepthBiasClamp        = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        rasterizerDesc.SlopeScaledDepthBias  = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rasterizerDesc.DepthClipEnable       = TRUE;
        rasterizerDesc.MultisampleEnable     = (sampleCount > 1) ? TRUE : FALSE;
        rasterizerDesc.AntialiasedLineEnable = FALSE;
        rasterizerDesc.ForcedSampleCount     = 0;
        rasterizerDesc.ConservativeRaster    = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        // Configure blend state
        D3D12_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable  = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;

        const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {
            TRUE, FALSE,
            D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD,
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
            depthStencilDesc.DepthWriteMask = depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
            depthStencilDesc.DepthFunc      = depthFunc;
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
        psoDesc.SampleDesc.Count      = sampleCount;
        psoDesc.SampleDesc.Quality    = quality;

        HR_CHECK(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
    }
}

