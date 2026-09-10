// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "DxCheck.h"

#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <span>

namespace Sandbox3D::Renderer
{
    using Microsoft::WRL::ComPtr;

    // Encapsulates a Direct3D 12 index buffer residing in upload memory
    class IndexBuffer final
    {
    public:
        IndexBuffer() = default;
        ~IndexBuffer() = default;

        IndexBuffer(const IndexBuffer&) = delete;
        IndexBuffer& operator=(const IndexBuffer&) = delete;
        IndexBuffer(IndexBuffer&&) noexcept = default;
        IndexBuffer& operator=(IndexBuffer&&) noexcept = default;

        // Initialise with 16-bit indices (DXGI_FORMAT_R16_UINT)
        void Initialise(ID3D12Device* device, std::span<const uint16_t> indices);

        // Initialise with 32-bit indices (DXGI_FORMAT_R32_UINT)
        void Initialise(ID3D12Device* device, std::span<const uint32_t> indices);

        [[nodiscard]] const D3D12_INDEX_BUFFER_VIEW& GetView() const noexcept { return m_bufferView; }
        [[nodiscard]] UINT GetIndexCount() const noexcept { return m_indexCount; }
        [[nodiscard]] DXGI_FORMAT GetFormat() const noexcept { return m_bufferView.Format; }

    private:
        void CreateResource(ID3D12Device* device, const void* data, size_t byteSize, DXGI_FORMAT format, UINT indexCount);

    private:
        ComPtr<ID3D12Resource>    m_uploadBuffer;
        D3D12_INDEX_BUFFER_VIEW   m_bufferView{};
        UINT                      m_indexCount{ 0 };
    };
}

