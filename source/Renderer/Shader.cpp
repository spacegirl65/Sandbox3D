// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Shader.h"

#include <stdexcept>
#include <format>
#include <fstream>
#include <sstream>

namespace Sandbox3D::Renderer
{
    static const char* GetTargetProfile(ShaderStage stage) noexcept
    {
        switch (stage)
        {
        case ShaderStage::Vertex:
            return "vs_5_0";
        case ShaderStage::Pixel:
            return "ps_5_0";
        default:
            return "vs_5_0";
        }
    }

    void Shader::CompileFromFile(
        const std::filesystem::path& filepath,
        const std::string& entryPoint,
        ShaderStage stage
    )
    {
        std::ifstream file(filepath);
        if (!file.is_open())
        {
            throw std::runtime_error(std::format("Failed to open HLSL shader file: {}", filepath.string()));
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        CompileFromSource(buffer.str(), filepath.string(), entryPoint, stage);
    }

    void Shader::CompileFromSource(
        const std::string& sourceCode,
        const std::string& sourceName,
        const std::string& entryPoint,
        ShaderStage stage
    )
    {
        UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
        compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        compileFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

        ComPtr<ID3DBlob> errorBlob;
        const char* targetProfile = GetTargetProfile(stage);

        const HRESULT hr = D3DCompile(
            sourceCode.data(),
            sourceCode.size(),
            sourceName.c_str(),
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entryPoint.c_str(),
            targetProfile,
            compileFlags,
            0,
            &m_bytecode,
            &errorBlob
        );

        if (FAILED(hr))
        {
            std::string errorMsg = "Unknown shader compilation error.";
            if (errorBlob)
            {
                errorMsg = static_cast<const char*>(errorBlob->GetBufferPointer());
            }

            OutputDebugStringA(std::format("[Shader Compilation Error in '{}']:\n{}\n", sourceName, errorMsg).c_str());
            throw std::runtime_error(std::format("Shader compilation failed for '{}':\n{}", sourceName, errorMsg));
        }
    }

    const void* Shader::GetBufferPointer() const noexcept
    {
        return m_bytecode ? m_bytecode->GetBufferPointer() : nullptr;
    }

    size_t Shader::GetBufferSize() const noexcept
    {
        return m_bytecode ? m_bytecode->GetBufferSize() : 0;
    }

    D3D12_SHADER_BYTECODE Shader::GetBytecode() const noexcept
    {
        D3D12_SHADER_BYTECODE desc = {};
        if (m_bytecode)
        {
            desc.pShaderBytecode = m_bytecode->GetBufferPointer();
            desc.BytecodeLength  = m_bytecode->GetBufferSize();
        }
        return desc;
    }
}

