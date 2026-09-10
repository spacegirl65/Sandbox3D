// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Mesh.h"

namespace Sandbox3D::Renderer
{
    void Mesh::Initialise(ID3D12Device* device, std::span<const Vertex> vertices)
    {
        m_vertexBuffer.Initialise(device, vertices);
        m_boundingBox    = m_vertexBuffer.GetBoundingBox();
        m_boundingSphere = m_vertexBuffer.GetBoundingSphere();
        m_isIndexed      = false;
    }

    void Mesh::Initialise(ID3D12Device* device, std::span<const Vertex> vertices, std::span<const uint16_t> indices)
    {
        m_vertexBuffer.Initialise(device, vertices);
        m_indexBuffer.Initialise(device, indices);
        m_boundingBox    = m_vertexBuffer.GetBoundingBox();
        m_boundingSphere = m_vertexBuffer.GetBoundingSphere();
        m_isIndexed      = true;
    }

    void Mesh::Initialise(ID3D12Device* device, std::span<const Vertex> vertices, std::span<const uint32_t> indices)
    {
        m_vertexBuffer.Initialise(device, vertices);
        m_indexBuffer.Initialise(device, indices);
        m_boundingBox    = m_vertexBuffer.GetBoundingBox();
        m_boundingSphere = m_vertexBuffer.GetBoundingSphere();
        m_isIndexed      = true;
    }

    void Mesh::Draw(ID3D12GraphicsCommandList* commandList) const noexcept
    {
        if (!IsInitialised())
        {
            return;
        }

        const D3D12_VERTEX_BUFFER_VIEW vbView = m_vertexBuffer.GetView();
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->IASetVertexBuffers(0, 1, &vbView);

        if (m_isIndexed)
        {
            const D3D12_INDEX_BUFFER_VIEW ibView = m_indexBuffer.GetView();
            commandList->IASetIndexBuffer(&ibView);
            commandList->DrawIndexedInstanced(m_indexBuffer.GetIndexCount(), 1, 0, 0, 0);
        }
        else
        {
            commandList->DrawInstanced(m_vertexBuffer.GetVertexCount(), 1, 0, 0);
        }
    }

    std::shared_ptr<Mesh> Mesh::CreateRedAndBlueQuad(ID3D12Device* device)
    {
        auto mesh = std::make_shared<Mesh>();

        // Origin-centred quad [-0.5, 0.5] with 6 distinct vertices for solid red and blue triangles
        const Vec3 quadNormal(0.0f, 0.0f, -1.0f);
        const Vertex quadVertices[6] = {
            // Red Triangle (Clockwise winding)
            { Vec3(-0.5f, -0.5f, 0.0f), quadNormal, Vec4::Red() },
            { Vec3(-0.5f,  0.5f, 0.0f), quadNormal, Vec4::Red() },
            { Vec3( 0.5f,  0.5f, 0.0f), quadNormal, Vec4::Red() },

            // Blue Triangle (Clockwise winding)
            { Vec3(-0.5f, -0.5f, 0.0f), quadNormal, Vec4::Blue() },
            { Vec3( 0.5f,  0.5f, 0.0f), quadNormal, Vec4::Blue() },
            { Vec3( 0.5f, -0.5f, 0.0f), quadNormal, Vec4::Blue() }
        };

        const uint16_t quadIndices[6] = { 0, 1, 2, 3, 4, 5 };
        mesh->Initialise(device, quadVertices, quadIndices);
        return mesh;
    }

    std::shared_ptr<Mesh> Mesh::CreateQuad(
        ID3D12Device* device,
        float width,
        float height,
        const Vec4& color
    )
    {
        auto mesh = std::make_shared<Mesh>();

        const float halfW = width * 0.5f;
        const float halfH = height * 0.5f;
        const Vec3 quadNormal(0.0f, 0.0f, -1.0f);

        const Vertex vertices[4] = {
            { Vec3(-halfW, -halfH, 0.0f), quadNormal, color }, // Bottom-Left
            { Vec3(-halfW,  halfH, 0.0f), quadNormal, color }, // Top-Left
            { Vec3( halfW,  halfH, 0.0f), quadNormal, color }, // Top-Right
            { Vec3( halfW, -halfH, 0.0f), quadNormal, color }  // Bottom-Right
        };

        const uint16_t indices[6] = {
            0, 1, 2,
            0, 2, 3
        };

        mesh->Initialise(device, vertices, indices);
        return mesh;
    }

    std::shared_ptr<Mesh> Mesh::CreateCube(
        ID3D12Device* device,
        float size,
        const Vec4& color
    )
    {
        auto mesh = std::make_shared<Mesh>();

        const float h = size * 0.5f;

        // 24 vertices (4 per face) with outward-facing surface normals
        const Vec3 normalFront( 0.0f,  0.0f,  1.0f);
        const Vec3 normalBack ( 0.0f,  0.0f, -1.0f);
        const Vec3 normalTop  ( 0.0f,  1.0f,  0.0f);
        const Vec3 normalBottom(0.0f, -1.0f,  0.0f);
        const Vec3 normalRight( 1.0f,  0.0f,  0.0f);
        const Vec3 normalLeft (-1.0f,  0.0f,  0.0f);

        const Vertex vertices[24] = {
            // Front face (+Z in LH: facing forward)
            { Vec3(-h, -h,  h), normalFront, color }, { Vec3(-h,  h,  h), normalFront, color }, { Vec3( h,  h,  h), normalFront, color }, { Vec3( h, -h,  h), normalFront, color },
            // Back face (-Z)
            { Vec3( h, -h, -h), normalBack,  color }, { Vec3( h,  h, -h), normalBack,  color }, { Vec3(-h,  h, -h), normalBack,  color }, { Vec3(-h, -h, -h), normalBack,  color },
            // Top face (+Y)
            { Vec3(-h,  h,  h), normalTop,   color }, { Vec3(-h,  h, -h), normalTop,   color }, { Vec3( h,  h, -h), normalTop,   color }, { Vec3( h,  h,  h), normalTop,   color },
            // Bottom face (-Y)
            { Vec3(-h, -h, -h), normalBottom,color }, { Vec3(-h, -h,  h), normalBottom,color }, { Vec3( h, -h,  h), normalBottom,color }, { Vec3( h, -h, -h), normalBottom,color },
            // Right face (+X)
            { Vec3( h, -h,  h), normalRight, color }, { Vec3( h,  h,  h), normalRight, color }, { Vec3( h,  h, -h), normalRight, color }, { Vec3( h, -h, -h), normalRight, color },
            // Left face (-X)
            { Vec3(-h, -h, -h), normalLeft,  color }, { Vec3(-h,  h, -h), normalLeft,  color }, { Vec3(-h,  h,  h), normalLeft,  color }, { Vec3(-h, -h,  h), normalLeft,  color }
        };

        // Outward-facing clockwise winding order in DirectX Left-Handed screen space
        const uint16_t indices[36] = {
            0,  2,  1,  0,  3,  2,  // Front (+Z)
            4,  6,  5,  4,  7,  6,  // Back (-Z)
            8, 10,  9,  8, 11, 10,  // Top (+Y)
            12, 14, 13, 12, 15, 14, // Bottom (-Y)
            16, 18, 17, 16, 19, 18, // Right (+X)
            20, 22, 21, 20, 23, 22  // Left (-X)
        };

        mesh->Initialise(device, vertices, indices);
        return mesh;
    }
}

