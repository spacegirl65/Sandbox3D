// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "DxCheck.h"

#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <string>
#include <filesystem>

namespace Sandbox3D::Renderer
{
    using Microsoft::WRL::ComPtr;

    enum class ShaderStage
    {
        Vertex,
        Pixel
    };

    // Encapsulates HLSL shader compilation and bytecode management
    class Shader final
    {
    public:
        Shader() = default;
        ~Shader() = default;

        void CompileFromFile(
            const std::filesystem::path& filepath,
            const std::string& entryPoint,
            ShaderStage stage
        );

        void CompileFromSource(
            const std::string& sourceCode,
            const std::string& sourceName,
            const std::string& entryPoint,
            ShaderStage stage
        );

        [[nodiscard]] const void* GetBufferPointer() const noexcept;
        [[nodiscard]] size_t GetBufferSize() const noexcept;
        [[nodiscard]] D3D12_SHADER_BYTECODE GetBytecode() const noexcept;

    private:
        ComPtr<ID3DBlob> m_bytecode;
    };
}

