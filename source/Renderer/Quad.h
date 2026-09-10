// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Maths/Maths.h"

#include <d3d12.h>
#include <vector>

namespace Sandbox3D::Renderer
{
    using Maths::Vec3;
    using Maths::Vec4;
    using Maths::BoundingBox;
    using Maths::BoundingSphere;

    // Encapsulates a planar 2D/3D quad defined by vertices and indexed triangles
    class Quad final
    {
    public:
        Quad() = default;
        ~Quad() = default;

        Quad(const Quad&) = delete;
        Quad& operator=(const Quad&) = delete;
        Quad(Quad&&) noexcept = default;
        Quad& operator=(Quad&&) noexcept = default;

        // Initialises a quad composed of two independent triangles (Option A: 6 unique vertices).
        // Prevents colour bleeding/interpolation across the shared diagonal, producing solid distinct colours.
        // Triangle 1: p0, p1, p2 with color1
        // Triangle 2: p3, p4, p5 with color2
        void InitialiseTwoTriangles(
            ID3D12Device* device,
            const Vec3& t1_v0, const Vec3& t1_v1, const Vec3& t1_v2, const Vec4& t1_color,
            const Vec3& t2_v0, const Vec3& t2_v1, const Vec3& t2_v2, const Vec4& t2_color
        );

        // Initialises a quad with 4 shared corner vertices and 6 indices
        // Useful when smooth corner interpolation or uniform colour is desired
        void InitialiseShared(
            ID3D12Device* device,
            const Vec3& bottomLeft,
            const Vec3& topLeft,
            const Vec3& topRight,
            const Vec3& bottomRight,
            const Vec4& bottomLeftColor,
            const Vec4& topLeftColor,
            const Vec4& topRightColor,
            const Vec4& bottomRightColor
        );

        // Binds vertex and index buffers and issues the indexed draw call
        void Draw(ID3D12GraphicsCommandList* commandList) const noexcept;

        [[nodiscard]] bool IsInitialised() const noexcept { return m_vertexBuffer.GetVertexCount() > 0; }
        [[nodiscard]] const VertexBuffer& GetVertexBuffer() const noexcept { return m_vertexBuffer; }
        [[nodiscard]] const IndexBuffer& GetIndexBuffer() const noexcept { return m_indexBuffer; }
        [[nodiscard]] const BoundingBox& GetBoundingBox() const noexcept { return m_vertexBuffer.GetBoundingBox(); }
        [[nodiscard]] const BoundingSphere& GetBoundingSphere() const noexcept { return m_vertexBuffer.GetBoundingSphere(); }

        // Factory helper creating the square quad centered at the origin (Option 1):
        // - One solid Red triangle:  (-0.5, -0.5), (-0.5, 0.5), (0.5, 0.5)
        // - One solid Blue triangle: (-0.5, -0.5), (0.5, 0.5), (0.5, -0.5)
        // Implemented with Option A (6 distinct vertices) to guarantee clean, sharp triangle colours.
        [[nodiscard]] static Quad CreateRedAndBlueQuad(ID3D12Device* device);

    private:
        VertexBuffer m_vertexBuffer;
        IndexBuffer  m_indexBuffer;
    };
}

