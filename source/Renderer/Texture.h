// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "DxCheck.h"

#include <d3d12.h>
#include <dxgiformat.h>
#include <wrl/client.h>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace Sandbox3D::Renderer
{
    using Microsoft::WRL::ComPtr;

    // Encapsulates a D3D12 GPU 2D texture resource, its subresource mip chain, and Shader Resource View
    class Texture
    {
    public:
        Texture() = default;
        ~Texture() = default;

        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;
        Texture(Texture&&) noexcept = default;
        Texture& operator=(Texture&&) noexcept = default;

        // Loads a DDS texture file from disk into a default GPU heap resource, recording upload commands
        [[nodiscard]] static std::shared_ptr<Texture> LoadFromDds(
            ID3D12Device* device,
            ID3D12GraphicsCommandList* commandList,
            const std::filesystem::path& filePath,
            ComPtr<ID3D12Resource>& outStagingBuffer,
            std::string_view debugName = ""
        );

        // Creates a Shader Resource View (SRV) for this texture at the given CPU descriptor handle
        void CreateShaderResourceView(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) const;

        [[nodiscard]] ID3D12Resource* GetResource() const noexcept { return m_resource.Get(); }
        [[nodiscard]] uint32_t GetWidth() const noexcept { return m_width; }
        [[nodiscard]] uint32_t GetHeight() const noexcept { return m_height; }
        [[nodiscard]] uint16_t GetMipLevels() const noexcept { return m_mipLevels; }
        [[nodiscard]] DXGI_FORMAT GetFormat() const noexcept { return m_format; }
        [[nodiscard]] const std::string& GetDebugName() const noexcept { return m_debugName; }

    private:
        ComPtr<ID3D12Resource> m_resource;
        uint32_t               m_width{ 0 };
        uint32_t               m_height{ 0 };
        uint16_t               m_mipLevels{ 1 };
        DXGI_FORMAT            m_format{ DXGI_FORMAT_UNKNOWN };
        std::string            m_debugName;
    };
}

