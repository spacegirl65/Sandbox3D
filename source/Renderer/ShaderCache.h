// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Shader.h"

#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_set>
#include <cstdint>

namespace Sandbox3D::Renderer
{
    using Microsoft::WRL::ComPtr;

    // Header layout written to the front of every binary shader cache (.cso) artifact
    #pragma pack(push, 1)
    struct ShaderCacheHeader
    {
        char     magic[4]            = { 'S', '3', 'S', 'C' };
        uint32_t version             = 1;
        uint32_t stage               = 0;
        uint32_t configHash          = 0;
        char     profile[8]          = {};
        uint64_t dependencyTimestamp = 0;
        uint64_t dependencyHash      = 0;
        uint64_t bytecodeSize        = 0;
        uint8_t  reserved[16]        = {};
    };
    #pragma pack(pop)

    static_assert(sizeof(ShaderCacheHeader) == 64, "ShaderCacheHeader must be exactly 64 bytes.");

    // Manages bytecode disc persistence, include dependency tracking, and future asset system interop
    class ShaderCache final
    {
    public:
        // Global singleton access for runtime renderer and asset provider pipelines
        static ShaderCache& Get() noexcept;

        ShaderCache();
        ~ShaderCache() = default;

        ShaderCache(const ShaderCache&) = delete;
        ShaderCache& operator=(const ShaderCache&) = delete;
        ShaderCache(ShaderCache&&) noexcept = default;
        ShaderCache& operator=(ShaderCache&&) noexcept = default;

        // Configures primary working cache folder in intermediate workspace
        void SetCacheDirectory(std::filesystem::path directory);
        [[nodiscard]] const std::filesystem::path& GetCacheDirectory() const noexcept;

        // Configures secondary cooked asset folder for pre-baked project releases
        void SetCookedDirectory(std::filesystem::path directory);
        [[nodiscard]] const std::filesystem::path& GetCookedDirectory() const noexcept;

        // Toggles strict cooked runtime mode (disallowing live compilation if cache is absent)
        void SetCookedMode(bool isCooked) noexcept;
        [[nodiscard]] bool IsCookedMode() const noexcept;

        // Resolves pre-compiled bytecode from disc or compiles fresh source and caches the artifact
        [[nodiscard]] ComPtr<ID3DBlob> LoadOrCompile(
            const std::filesystem::path& filepath,
            const std::string& entryPoint,
            ShaderStage stage
        );

        // Forces invalidation of a specific cached artifact
        bool InvalidateCache(
            const std::filesystem::path& filepath,
            const std::string& entryPoint,
            ShaderStage stage
        );

        // Discovers recursive #include dependencies for a given HLSL source file
        [[nodiscard]] std::vector<std::filesystem::path> ResolveDependencies(
            const std::filesystem::path& filepath
        ) const;

    private:
        // Evaluates unique cache file target path in designated cache directory
        [[nodiscard]] std::filesystem::path BuildCachePath(
            const std::filesystem::path& directory,
            const std::filesystem::path& filepath,
            const std::string& entryPoint,
            ShaderStage stage,
            uint32_t configHash
        ) const;

        // Calculates build configuration hash based on compiler flags and preprocessor macros
        [[nodiscard]] uint32_t ComputeConfigurationHash(ShaderStage stage) const noexcept;

        // Evaluates maximum write timestamp and content hash across dependency graph
        void EvaluateDependencyMetrics(
            const std::vector<std::filesystem::path>& dependencies,
            uint64_t& outMaxTimestamp,
            uint64_t& outContentHash
        ) const;

        // Reads bytecode from binary container after validating header signature and timestamps
        [[nodiscard]] ComPtr<ID3DBlob> ReadBytecodeFromCache(
            const std::filesystem::path& cachePath,
            ShaderStage stage,
            uint32_t configHash,
            uint64_t currentDependencyTimestamp,
            uint64_t currentDependencyHash
        ) const;

        // Writes compiled bytecode along with header metadata container to disc
        void WriteBytecodeToCache(
            const std::filesystem::path& cachePath,
            ID3DBlob* bytecodeBlob,
            ShaderStage stage,
            uint32_t configHash,
            uint64_t dependencyTimestamp,
            uint64_t dependencyHash
        ) const;

        std::filesystem::path m_cacheDirectory;
        std::filesystem::path m_cookedDirectory;
        bool                  m_isCookedMode{ false };
    };
}
