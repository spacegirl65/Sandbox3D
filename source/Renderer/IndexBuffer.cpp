// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "IndexBuffer.h"

#include <cstring>
#include <stdexcept>

namespace Sandbox3D::Renderer
{
    void IndexBuffer::Initialise(ID3D12Device* device, std::span<const uint16_t> indices)
    {
        if (indices.empty())
        {
            throw std::runtime_error("Cannot initialise IndexBuffer with empty index span.");
        }

        CreateResource(
            device,
            indices.data(),
            indices.size_bytes(),
            DXGI_FORMAT_R16_UINT,
            static_cast<UINT>(indices.size())
        );
    }

    void IndexBuffer::Initialise(ID3D12Device* device, std::span<const uint32_t> indices)
    {
        if (indices.empty())
        {
            throw std::runtime_error("Cannot initialise IndexBuffer with empty index span.");
        }

        CreateResource(
            device,
            indices.data(),
            indices.size_bytes(),
            DXGI_FORMAT_R32_UINT,
            static_cast<UINT>(indices.size())
        );
    }

    void IndexBuffer::CreateResource(
        ID3D12Device* device,
        const void* data,
        size_t byteSize,
        DXGI_FORMAT format,
        UINT indexCount
    )
    {
        m_indexCount = indexCount;

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type                 = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask     = 1;
        heapProps.VisibleNodeMask      = 1;

        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Alignment          = 0;
        bufferDesc.Width              = byteSize;
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

        // Copy index data to upload buffer
        void* mappedData = nullptr;
        D3D12_RANGE readRange = { 0, 0 };
        HR_CHECK(m_uploadBuffer->Map(0, &readRange, &mappedData));
        std::memcpy(mappedData, data, byteSize);
        m_uploadBuffer->Unmap(0, nullptr);

        // Initialise index buffer view
        m_bufferView.BufferLocation = m_uploadBuffer->GetGPUVirtualAddress();
        m_bufferView.SizeInBytes    = static_cast<UINT>(byteSize);
        m_bufferView.Format         = format;
    }
}

