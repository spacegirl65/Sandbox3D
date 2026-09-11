// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "DxCheck.h"

#include <d3d12.h>
#include <wrl/client.h>
#include <concepts>
#include <cstring>

namespace Sandbox3D::Renderer
{
    using Microsoft::WRL::ComPtr;

    // Type-safe 256-byte aligned upload constant buffer for Direct3D 12 supporting multi-element suballocations
    template <typename T>
    class ConstantBuffer final
    {
    public:
        static constexpr size_t CalculateAlignedSize() noexcept
        {
            return (sizeof(T) + 255) & ~255;
        }

        ConstantBuffer() = default;
        ~ConstantBuffer()
        {
            Shutdown();
        }

        ConstantBuffer(const ConstantBuffer&) = delete;
        ConstantBuffer& operator=(const ConstantBuffer&) = delete;
        ConstantBuffer(ConstantBuffer&&) noexcept = default;
        ConstantBuffer& operator=(ConstantBuffer&&) noexcept = default;

        void Initialise(ID3D12Device* device, size_t maxElements = 1)
        {
            m_maxElements = (maxElements == 0) ? 1 : maxElements;
            const size_t alignedSize = CalculateAlignedSize();
            const size_t totalBufferSize = alignedSize * m_maxElements;

            D3D12_HEAP_PROPERTIES heapProps = {};
            heapProps.Type                 = D3D12_HEAP_TYPE_UPLOAD;
            heapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            heapProps.CreationNodeMask     = 1;
            heapProps.VisibleNodeMask      = 1;

            D3D12_RESOURCE_DESC bufferDesc = {};
            bufferDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
            bufferDesc.Alignment          = 0;
            bufferDesc.Width              = totalBufferSize;
            bufferDesc.Height             = 1;
            bufferDesc.DepthOrArraySize   = 1;
            bufferDesc.MipLevels          = 1;
            bufferDesc.Format             = DXGI_FORMAT_UNKNOWN;
            bufferDesc.SampleDesc.Count   = 1;
            bufferDesc.SampleDesc.Quality = 0;
            bufferDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            bufferDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;

            HR_CHECK(device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &bufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&m_uploadBuffer)
            ));

            // Keep buffer mapped for upload heap throughout its lifetime
            D3D12_RANGE readRange = { 0, 0 }; // CPU does not read from this resource
            HR_CHECK(m_uploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_mappedData)));
        }

        void Shutdown() noexcept
        {
            if (m_uploadBuffer)
            {
                if (m_mappedData)
                {
                    m_uploadBuffer->Unmap(0, nullptr);
                    m_mappedData = nullptr;
                }
                m_uploadBuffer.Reset();
            }
            m_maxElements = 0;
        }

        void Update(const T& data, size_t index = 0) noexcept
        {
            if (m_mappedData && index < m_maxElements)
            {
                const size_t offset = index * CalculateAlignedSize();
                std::memcpy(m_mappedData + offset, &data, sizeof(T));
            }
        }

        [[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GetGpuVirtualAddress(size_t index = 0) const noexcept
        {
            if (!m_uploadBuffer || index >= m_maxElements)
            {
                return 0;
            }
            return m_uploadBuffer->GetGPUVirtualAddress() + index * CalculateAlignedSize();
        }

        [[nodiscard]] size_t GetMaxElements() const noexcept { return m_maxElements; }

    private:
        ComPtr<ID3D12Resource> m_uploadBuffer;
        uint8_t*               m_mappedData{ nullptr };
        size_t                 m_maxElements{ 0 };
    };
}

