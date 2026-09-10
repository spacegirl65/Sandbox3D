// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Quad.h"

namespace Sandbox3D::Renderer
{
    void Quad::InitialiseTwoTriangles(
        ID3D12Device* device,
        const Vec3& t1_v0, const Vec3& t1_v1, const Vec3& t1_v2, const Vec4& t1_color,
        const Vec3& t2_v0, const Vec3& t2_v1, const Vec3& t2_v2, const Vec4& t2_color
    )
    {
        // 6 distinct vertices (Option A) ensuring zero attribute bleeding across triangle edges:
        // Triangle 1: Vertices 0, 1, 2
        // Triangle 2: Vertices 3, 4, 5
        const Vertex quadVertices[6] = {
            // Triangle 1
            { t1_v0, t1_color },
            { t1_v1, t1_color },
            { t1_v2, t1_color },

            // Triangle 2
            { t2_v0, t2_color },
            { t2_v1, t2_color },
            { t2_v2, t2_color }
        };

        const uint16_t quadIndices[6] = {
            0, 1, 2,
            3, 4, 5
        };

        m_vertexBuffer.Initialise(device, quadVertices);
        m_indexBuffer.Initialise(device, quadIndices);
    }

    void Quad::InitialiseShared(
        ID3D12Device* device,
        const Vec3& bottomLeft,
        const Vec3& topLeft,
        const Vec3& topRight,
        const Vec3& bottomRight,
        const Vec4& bottomLeftColor,
        const Vec4& topLeftColor,
        const Vec4& topRightColor,
        const Vec4& bottomRightColor
    )
    {
        // 4 shared corner vertices:
        // Vertex 0: Bottom-Left
        // Vertex 1: Top-Left
        // Vertex 2: Top-Right
        // Vertex 3: Bottom-Right
        const Vertex quadVertices[4] = {
            { bottomLeft,  bottomLeftColor },
            { topLeft,     topLeftColor },
            { topRight,    topRightColor },
            { bottomRight, bottomRightColor }
        };

        // Two triangles sharing the diagonal from Vertex 0 to Vertex 2 (clockwise winding):
        // Triangle 1: 0 -> 1 -> 2
        // Triangle 2: 0 -> 2 -> 3
        const uint16_t quadIndices[6] = {
            0, 1, 2,
            0, 2, 3
        };

        m_vertexBuffer.Initialise(device, quadVertices);
        m_indexBuffer.Initialise(device, quadIndices);
    }

    void Quad::Draw(ID3D12GraphicsCommandList* commandList) const noexcept
    {
        if (!IsInitialised())
        {
            return;
        }

        const D3D12_VERTEX_BUFFER_VIEW vbView = m_vertexBuffer.GetView();
        const D3D12_INDEX_BUFFER_VIEW  ibView = m_indexBuffer.GetView();

        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->IASetVertexBuffers(0, 1, &vbView);
        commandList->IASetIndexBuffer(&ibView);
        commandList->DrawIndexedInstanced(m_indexBuffer.GetIndexCount(), 1, 0, 0, 0);
    }

    Quad Quad::CreateRedAndBlueQuad(ID3D12Device* device)
    {
        // Option 1: Quad centered at the origin (0, 0, 0) spanning [-0.5, 0.5] on X and Y
        // Red Triangle:  (-0.5, -0.5), (-0.5, 0.5), (0.5, 0.5)
        // Blue Triangle: (-0.5, -0.5), (0.5, 0.5), (0.5, -0.5)
        // Top-right corner of the quad is at (0.5, 0.5, 0.0).
        // Using Option A (6 distinct vertices) to guarantee pure solid colours with no gradient interpolation.
        Quad quad;
        quad.InitialiseTwoTriangles(
            device,
            // Red Triangle (Clockwise winding)
            Vec3(-0.5f, -0.5f, 0.0f),
            Vec3(-0.5f,  0.5f, 0.0f),
            Vec3( 0.5f,  0.5f, 0.0f),
            Vec4::Red(),

            // Blue Triangle (Clockwise winding)
            Vec3(-0.5f, -0.5f, 0.0f),
            Vec3( 0.5f,  0.5f, 0.0f),
            Vec3( 0.5f, -0.5f, 0.0f),
            Vec4::Blue()
        );

        return quad;
    }
}

