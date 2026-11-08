// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "ShaderCache.h"
#include "DxCheck.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <format>
#include <algorithm>
#include <cstring>

namespace Sandbox3D::Renderer
{
    namespace
    {
        constexpr const char* HeaderMagicString = "S3SC";
        constexpr uint32_t CurrentCacheFormatVersion = 1u;
        constexpr const char* DefaultCacheFolder = "intermediate/cache/shaders";

        [[nodiscard]] const char* GetTargetProfile(ShaderStage stage) noexcept
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

        [[nodiscard]] uint32_t GetCompilationFlags() noexcept
        {
            uint32_t flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;
#if defined(_DEBUG)
            flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
            flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
            return flags;
        }

        [[nodiscard]] uint64_t HashFnv1a64(const void* data, size_t size, uint64_t seed = 14695981039346656037ull) noexcept
        {
            const auto* bytes = static_cast<const uint8_t*>(data);
            uint64_t hash = seed;
            for (size_t i = 0; i < size; ++i)
            {
                hash ^= static_cast<uint64_t>(bytes[i]);
                hash *= 1099511628211ull;
            }
            return hash;
        }

        [[nodiscard]] uint32_t HashFnv1a32(const void* data, size_t size, uint32_t seed = 2166136261u) noexcept
        {
            const auto* bytes = static_cast<const uint8_t*>(data);
            uint32_t hash = seed;
            for (size_t i = 0; i < size; ++i)
            {
                hash ^= static_cast<uint32_t>(bytes[i]);
                hash *= 16777619u;
            }
            return hash;
        }
    }

    ShaderCache& ShaderCache::Get() noexcept
    {
        static ShaderCache instance;
        return instance;
    }

    ShaderCache::ShaderCache()
        : m_cacheDirectory(DefaultCacheFolder)
        , m_cookedDirectory("resources/shaders")
        , m_isCookedMode(false)
    {
    }

    void ShaderCache::SetCacheDirectory(std::filesystem::path directory)
    {
        m_cacheDirectory = std::move(directory);
    }

    const std::filesystem::path& ShaderCache::GetCacheDirectory() const noexcept
    {
        return m_cacheDirectory;
    }

    void ShaderCache::SetCookedDirectory(std::filesystem::path directory)
    {
        m_cookedDirectory = std::move(directory);
    }

    const std::filesystem::path& ShaderCache::GetCookedDirectory() const noexcept
    {
        return m_cookedDirectory;
    }

    void ShaderCache::SetCookedMode(bool isCooked) noexcept
    {
        m_isCookedMode = isCooked;
    }

    bool ShaderCache::IsCookedMode() const noexcept
    {
        return m_isCookedMode;
    }

    uint32_t ShaderCache::ComputeConfigurationHash(ShaderStage stage) const noexcept
    {
        const uint32_t flags = GetCompilationFlags();
        const char* profile = GetTargetProfile(stage);

        struct ConfigContext
        {
            uint32_t compileFlags;
            char targetProfile[8];
        } context{};

        context.compileFlags = flags;
        const size_t len = std::min(std::strlen(profile), sizeof(context.targetProfile) - 1);
        std::memcpy(context.targetProfile, profile, len);
        context.targetProfile[len] = '\0';

        return HashFnv1a32(&context, sizeof(context));
    }

    std::vector<std::filesystem::path> ShaderCache::ResolveDependencies(
        const std::filesystem::path& filepath
    ) const
    {
        std::vector<std::filesystem::path> dependencies;
        std::unordered_set<std::string> visited;

        const auto resolveRecursive = [&](const std::filesystem::path& path, auto& self) -> void
        {
            std::error_code canonicalEc;
            const std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(path, canonicalEc);
            const std::string canonicalKey = canonicalEc ? path.string() : canonicalPath.string();

            if (visited.contains(canonicalKey))
            {
                return;
            }
            visited.insert(canonicalKey);
            dependencies.push_back(path);

            std::ifstream file(path);
            if (!file.is_open())
            {
                return;
            }

            const std::filesystem::path parentDir = path.parent_path();
            std::string line;

            while (std::getline(file, line))
            {
                const size_t includePos = line.find("#include");
                if (includePos == std::string::npos)
                {
                    continue;
                }

                // Locate opening quote or angle bracket following the #include directive
                const size_t openBracket = line.find_first_of("\"<", includePos + 8);
                if (openBracket == std::string::npos)
                {
                    continue;
                }

                const char closeChar = (line[openBracket] == '"') ? '"' : '>';
                const size_t closeBracket = line.find(closeChar, openBracket + 1);
                if (closeBracket == std::string::npos || closeBracket <= openBracket + 1)
                {
                    continue;
                }

                const std::string includeName = line.substr(openBracket + 1, closeBracket - openBracket - 1);
                std::filesystem::path resolvedInclude = parentDir / includeName;
                if (!std::filesystem::exists(resolvedInclude))
                {
                    resolvedInclude = filepath.parent_path() / includeName;
                }

                if (std::filesystem::exists(resolvedInclude))
                {
                    self(resolvedInclude, self);
                }
            }
        };

        if (std::filesystem::exists(filepath))
        {
            resolveRecursive(filepath, resolveRecursive);
        }

        return dependencies;
    }

    void ShaderCache::EvaluateDependencyMetrics(
        const std::vector<std::filesystem::path>& dependencies,
        uint64_t& outMaxTimestamp,
        uint64_t& outContentHash
    ) const
    {
        outMaxTimestamp = 0;
        outContentHash  = 14695981039346656037ull;

        for (const auto& dep : dependencies)
        {
            if (!std::filesystem::exists(dep))
            {
                continue;
            }

            std::error_code ec;
            const auto lastWrite = std::filesystem::last_write_time(dep, ec);
            if (!ec)
            {
                const auto writeTicks = static_cast<uint64_t>(lastWrite.time_since_epoch().count());
                outMaxTimestamp = std::max(outMaxTimestamp, writeTicks);
            }

            std::ifstream stream(dep, std::ios::binary);
            if (stream.is_open())
            {
                std::string content((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
                outContentHash = HashFnv1a64(content.data(), content.size(), outContentHash);
            }
        }
    }

    std::filesystem::path ShaderCache::BuildCachePath(
        const std::filesystem::path& directory,
        const std::filesystem::path& filepath,
        const std::string& entryPoint,
        ShaderStage stage,
        uint32_t configHash
    ) const
    {
        const std::string stem = filepath.stem().string();
        const std::string profile = GetTargetProfile(stage);
#if defined(_DEBUG)
        constexpr const char* configTag = "Debug";
#else
        constexpr const char* configTag = "Release";
#endif
        const std::string fullIdentifier = std::format("{}_{}_{}_{}", filepath.string(), entryPoint, profile, configTag);
        const uint32_t identHash = HashFnv1a32(fullIdentifier.data(), fullIdentifier.size());

        const std::string cacheFilename = std::format(
            "{}_{}_{}_{}_{:08x}.cso",
            stem,
            entryPoint,
            profile,
            configTag,
            identHash
        );

        return directory / cacheFilename;
    }

    ComPtr<ID3DBlob> ShaderCache::ReadBytecodeFromCache(
        const std::filesystem::path& cachePath,
        ShaderStage stage,
        uint32_t configHash,
        uint64_t currentDependencyTimestamp,
        uint64_t currentDependencyHash
    ) const
    {
        std::error_code ec;
        if (!std::filesystem::exists(cachePath, ec))
        {
            return nullptr;
        }

        const uintmax_t fileSize = std::filesystem::file_size(cachePath, ec);
        if (ec || fileSize < sizeof(ShaderCacheHeader))
        {
            return nullptr;
        }

        std::ifstream file(cachePath, std::ios::binary);
        if (!file.is_open())
        {
            return nullptr;
        }

        ShaderCacheHeader header{};
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!file)
        {
            return nullptr;
        }

        // Validate header identity and structure integrity
        if (std::memcmp(header.magic, HeaderMagicString, 4) != 0 || header.version != CurrentCacheFormatVersion)
        {
            return nullptr;
        }

        // Validate shader stage and profile matching
        if (header.stage != static_cast<uint32_t>(stage))
        {
            return nullptr;
        }

        const char* expectedProfile = GetTargetProfile(stage);
        if (std::strncmp(header.profile, expectedProfile, sizeof(header.profile)) != 0)
        {
            return nullptr;
        }

        // Validate compilation flags and preprocessor configuration
        if (header.configHash != configHash)
        {
            return nullptr;
        }

        // Validate include dependencies and source modification timestamps
        if (header.dependencyTimestamp != currentDependencyTimestamp || header.dependencyHash != currentDependencyHash)
        {
            return nullptr;
        }

        if (fileSize != sizeof(ShaderCacheHeader) + header.bytecodeSize)
        {
            return nullptr;
        }

        ComPtr<ID3DBlob> bytecodeBlob;
        const HRESULT hr = D3DCreateBlob(static_cast<SIZE_T>(header.bytecodeSize), &bytecodeBlob);
        if (FAILED(hr) || !bytecodeBlob)
        {
            return nullptr;
        }

        file.read(static_cast<char*>(bytecodeBlob->GetBufferPointer()), static_cast<std::streamsize>(header.bytecodeSize));
        if (!file)
        {
            return nullptr;
        }

        return bytecodeBlob;
    }

    void ShaderCache::WriteBytecodeToCache(
        const std::filesystem::path& cachePath,
        ID3DBlob* bytecodeBlob,
        ShaderStage stage,
        uint32_t configHash,
        uint64_t dependencyTimestamp,
        uint64_t dependencyHash
    ) const
    {
        if (!bytecodeBlob)
        {
            return;
        }

        std::error_code ec;
        std::filesystem::create_directories(cachePath.parent_path(), ec);

        std::ofstream file(cachePath, std::ios::binary | std::ios::trunc);
        if (!file.is_open())
        {
            std::wcerr << L"[ShaderCache] Warning: Unable to open file for writing: "
                       << cachePath.wstring() << L"\n";
            return;
        }

        ShaderCacheHeader header{};
        std::memcpy(header.magic, HeaderMagicString, 4);
        header.version             = CurrentCacheFormatVersion;
        header.stage               = static_cast<uint32_t>(stage);
        header.configHash          = configHash;
        header.dependencyTimestamp = dependencyTimestamp;
        header.dependencyHash      = dependencyHash;
        header.bytecodeSize        = static_cast<uint64_t>(bytecodeBlob->GetBufferSize());

        const char* profile = GetTargetProfile(stage);
        const size_t len = std::min(std::strlen(profile), sizeof(header.profile) - 1);
        std::memcpy(header.profile, profile, len);
        header.profile[len] = '\0';

        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        file.write(static_cast<const char*>(bytecodeBlob->GetBufferPointer()), static_cast<std::streamsize>(header.bytecodeSize));
        file.flush();
    }

    bool ShaderCache::InvalidateCache(
        const std::filesystem::path& filepath,
        const std::string& entryPoint,
        ShaderStage stage
    )
    {
        const uint32_t configHash = ComputeConfigurationHash(stage);
        const std::filesystem::path cachePath = BuildCachePath(m_cacheDirectory, filepath, entryPoint, stage, configHash);

        std::error_code ec;
        if (std::filesystem::exists(cachePath, ec))
        {
            return std::filesystem::remove(cachePath, ec);
        }
        return false;
    }

    ComPtr<ID3DBlob> ShaderCache::LoadOrCompile(
        const std::filesystem::path& filepath,
        const std::string& entryPoint,
        ShaderStage stage
    )
    {
        const uint32_t configHash = ComputeConfigurationHash(stage);

        // Gather all recursive #include dependencies to construct cache validity metrics
        const std::vector<std::filesystem::path> dependencies = ResolveDependencies(filepath);
        uint64_t dependencyTimestamp = 0;
        uint64_t dependencyHash      = 0;
        EvaluateDependencyMetrics(dependencies, dependencyTimestamp, dependencyHash);

        // Attempt retrieval from pre-cooked asset directory first (if configured)
        if (!m_cookedDirectory.empty())
        {
            const std::filesystem::path cookedPath = BuildCachePath(m_cookedDirectory, filepath, entryPoint, stage, configHash);
            if (auto cookedBlob = ReadBytecodeFromCache(cookedPath, stage, configHash, dependencyTimestamp, dependencyHash))
            {
                std::wcout << L"[ShaderCache] Loaded pre-cooked shader file: " << cookedPath.filename().wstring() << L"\n";
                return cookedBlob;
            }
        }

        // Attempt retrieval from intermediate developer cache directory
        const std::filesystem::path cachePath = BuildCachePath(m_cacheDirectory, filepath, entryPoint, stage, configHash);
        if (auto cachedBlob = ReadBytecodeFromCache(cachePath, stage, configHash, dependencyTimestamp, dependencyHash))
        {
            std::wcout << L"[ShaderCache] Loaded cached shader file: " << cachePath.filename().wstring() << L"\n";
            return cachedBlob;
        }

        // Cache miss: verify whether runtime compilation is permitted under current execution mode
        if (m_isCookedMode)
        {
            throw std::runtime_error(std::format(
                "Shader cache miss in cooked export mode: '{}' [{}] cannot be compiled at runtime.",
                filepath.string(),
                entryPoint
            ));
        }

        // Read plain source code from disk
        std::ifstream file(filepath);
        if (!file.is_open())
        {
            throw std::runtime_error(std::format("Failed to open HLSL shader file: {}", filepath.string()));
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        const std::string sourceCode = buffer.str();

        const uint32_t compileFlags = GetCompilationFlags();
        const char* targetProfile = GetTargetProfile(stage);

        ComPtr<ID3DBlob> bytecodeBlob;
        ComPtr<ID3DBlob> errorBlob;

        const HRESULT hr = D3DCompile(
            sourceCode.data(),
            sourceCode.size(),
            filepath.string().c_str(),
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entryPoint.c_str(),
            targetProfile,
            compileFlags,
            0,
            &bytecodeBlob,
            &errorBlob
        );

        if (FAILED(hr))
        {
            std::string errorMsg = "Unknown shader compilation error.";
            if (errorBlob)
            {
                errorMsg = static_cast<const char*>(errorBlob->GetBufferPointer());
            }

            OutputDebugStringA(std::format("[Shader Compilation Error in '{}']:\n{}\n", filepath.string(), errorMsg).c_str());
            throw std::runtime_error(std::format("Shader compilation failed for '{}':\n{}", filepath.string(), errorMsg));
        }

        // Write newly compiled bytecode to disc cache
        WriteBytecodeToCache(cachePath, bytecodeBlob.Get(), stage, configHash, dependencyTimestamp, dependencyHash);

        std::wcout << L"[ShaderCache] Compiled HLSL shader file: " << filepath.filename().wstring()
            << L" [" << std::wstring(entryPoint.begin(), entryPoint.end()) << L"]...\n";

        return bytecodeBlob;
    }
}
