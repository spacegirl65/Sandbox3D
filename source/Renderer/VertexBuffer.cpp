// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "VertexBuffer.h"

#include <cstring>
#include <stdexcept>

namespace Sandbox3D::Renderer
{
    void VertexBuffer::Initialise(ID3D12Device* device, std::span<const Vertex> vertices)
    {
        if (vertices.empty())
        {
            throw std::runtime_error("Cannot initialise VertexBuffer with empty vertex span.");
        }

        m_vertexCount = static_cast<UINT>(vertices.size());
        const size_t bufferSize = vertices.size_bytes();

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type                 = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask     = 1;
        heapProps.VisibleNodeMask      = 1;

        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Alignment          = 0;
        bufferDesc.Width              = bufferSize;
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

        // Copy vertex data to upload buffer
        void* mappedData = nullptr;
        D3D12_RANGE readRange = { 0, 0 };
        HR_CHECK(m_uploadBuffer->Map(0, &readRange, &mappedData));
        std::memcpy(mappedData, vertices.data(), bufferSize);
        m_uploadBuffer->Unmap(0, nullptr);

        // Initialise vertex buffer view
        m_bufferView.BufferLocation = m_uploadBuffer->GetGPUVirtualAddress();
        m_bufferView.StrideInBytes  = sizeof(Vertex);
        m_bufferView.SizeInBytes    = static_cast<UINT>(bufferSize);
    }

    VertexBuffer VertexBuffer::CreateRedTriangle(ID3D12Device* device)
    {
        // Red color: (1.0f, 0.0f, 0.0f)
        const Vec3 red(1.0f, 0.0f, 0.0f);

        // Specified coordinates: (0, 0), (0, 1), and (1, 1)
        // Adheres to Clockwise winding order in DirectX Left-Handed screen coordinates
        const Vertex triangleVertices[3] = {
            { Vec3(0.0f, 0.0f, 0.0f), red },
            { Vec3(0.0f, 1.0f, 0.0f), red },
            { Vec3(1.0f, 1.0f, 0.0f), red }
        };

        VertexBuffer vb;
        vb.Initialise(device, triangleVertices);
        return vb;
    }
}

