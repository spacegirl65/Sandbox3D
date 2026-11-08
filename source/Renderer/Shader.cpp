// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Shader.h"
#include "ShaderCache.h"

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
        // Route through the persistent disk cache system to eliminate redundant runtime compilation
        m_bytecode = ShaderCache::Get().LoadOrCompile(filepath, entryPoint, stage);
    }

    void Shader::CompileFromSource(
        const std::string& sourceCode,
        const std::string& sourceName,
        const std::string& entryPoint,
        ShaderStage stage
    )
    {
        UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;
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

    void Shader::LoadFromBytecode(ComPtr<ID3DBlob> bytecode) noexcept
    {
        m_bytecode = std::move(bytecode);
    }

    void Shader::LoadFromBytecode(const void* data, size_t size)
    {
        if (!data || size == 0)
        {
            m_bytecode.Reset();
            return;
        }

        ComPtr<ID3DBlob> blob;
        const HRESULT hr = D3DCreateBlob(static_cast<SIZE_T>(size), &blob);
        if (FAILED(hr))
        {
            throw std::runtime_error("Failed to allocate shader bytecode blob.");
        }

        std::memcpy(blob->GetBufferPointer(), data, size);
        m_bytecode = std::move(blob);
    }

    void Shader::LoadFromBytecode(std::span<const uint8_t> bytecode)
    {
        LoadFromBytecode(bytecode.data(), bytecode.size());
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
