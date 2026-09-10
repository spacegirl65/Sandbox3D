// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "DxCheck.h"
#include "../Maths/Maths.h"

#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <span>

namespace Sandbox3D::Renderer
{
    using Microsoft::WRL::ComPtr;
    using Maths::Vec3;

    // Single vertex containing 3D position and RGB colour
    struct Vertex
    {
        Vec3 position;
        Vec3 color;
    };

    // Encapsulates a Direct3D 12 vertex buffer residing in upload memory
    class VertexBuffer final
    {
    public:
        VertexBuffer() = default;
        ~VertexBuffer() = default;

        VertexBuffer(const VertexBuffer&) = delete;
        VertexBuffer& operator=(const VertexBuffer&) = delete;
        VertexBuffer(VertexBuffer&&) noexcept = default;
        VertexBuffer& operator=(VertexBuffer&&) noexcept = default;

        void Initialise(ID3D12Device* device, std::span<const Vertex> vertices);

        [[nodiscard]] const D3D12_VERTEX_BUFFER_VIEW& GetView() const noexcept { return m_bufferView; }
        [[nodiscard]] UINT GetVertexCount() const noexcept { return m_vertexCount; }

        // Factory helper creating the requested red triangle with coordinates (0, 0), (0, 1), and (1, 1)
        [[nodiscard]] static VertexBuffer CreateRedTriangle(ID3D12Device* device);

    private:
        ComPtr<ID3D12Resource>    m_uploadBuffer;
        D3D12_VERTEX_BUFFER_VIEW  m_bufferView{};
        UINT                      m_vertexCount{ 0 };
    };
}

