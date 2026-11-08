// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "DxCheck.h"

#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <string>
#include <filesystem>
#include <span>

namespace Sandbox3D::Renderer
{
    using Microsoft::WRL::ComPtr;

    enum class ShaderStage
    {
        Vertex,
        Pixel
    };

    // Encapsulates HLSL shader compilation, disc bytecode caching, and bytecode management
    class Shader final
    {
    public:
        Shader() = default;
        ~Shader() = default;

        // Resolves bytecode from disc cache or compiles source through the ShaderCache system
        void CompileFromFile(
            const std::filesystem::path& filepath,
            const std::string& entryPoint,
            ShaderStage stage
        );

        // Compiles directly from memory string (used for embedded fallback shaders)
        void CompileFromSource(
            const std::string& sourceCode,
            const std::string& sourceName,
            const std::string& entryPoint,
            ShaderStage stage
        );

        // Ingests pre-compiled bytecode directly (used by asset management and cooked package loaders)
        void LoadFromBytecode(ComPtr<ID3DBlob> bytecode) noexcept;
        void LoadFromBytecode(const void* data, size_t size);
        void LoadFromBytecode(std::span<const uint8_t> bytecode);

        [[nodiscard]] const void* GetBufferPointer() const noexcept;
        [[nodiscard]] size_t GetBufferSize() const noexcept;
        [[nodiscard]] D3D12_SHADER_BYTECODE GetBytecode() const noexcept;

    private:
        ComPtr<ID3DBlob> m_bytecode;
    };
}
