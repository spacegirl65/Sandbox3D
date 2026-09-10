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
        const Vertex quadVertices[6] = {
            // Red Triangle (Clockwise winding)
            { Vec3(-0.5f, -0.5f, 0.0f), Vec4::Red() },
            { Vec3(-0.5f,  0.5f, 0.0f), Vec4::Red() },
            { Vec3( 0.5f,  0.5f, 0.0f), Vec4::Red() },

            // Blue Triangle (Clockwise winding)
            { Vec3(-0.5f, -0.5f, 0.0f), Vec4::Blue() },
            { Vec3( 0.5f,  0.5f, 0.0f), Vec4::Blue() },
            { Vec3( 0.5f, -0.5f, 0.0f), Vec4::Blue() }
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

        const Vertex vertices[4] = {
            { Vec3(-halfW, -halfH, 0.0f), color }, // Bottom-Left
            { Vec3(-halfW,  halfH, 0.0f), color }, // Top-Left
            { Vec3( halfW,  halfH, 0.0f), color }, // Top-Right
            { Vec3( halfW, -halfH, 0.0f), color }  // Bottom-Right
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

        // 24 vertices (4 per face) to allow face normals / colours
        const Vertex vertices[24] = {
            // Front face (+Z in LH: facing forward)
            { Vec3(-h, -h,  h), color }, { Vec3(-h,  h,  h), color }, { Vec3( h,  h,  h), color }, { Vec3( h, -h,  h), color },
            // Back face (-Z)
            { Vec3( h, -h, -h), color }, { Vec3( h,  h, -h), color }, { Vec3(-h,  h, -h), color }, { Vec3(-h, -h, -h), color },
            // Top face (+Y)
            { Vec3(-h,  h,  h), color }, { Vec3(-h,  h, -h), color }, { Vec3( h,  h, -h), color }, { Vec3( h,  h,  h), color },
            // Bottom face (-Y)
            { Vec3(-h, -h, -h), color }, { Vec3(-h, -h,  h), color }, { Vec3( h, -h,  h), color }, { Vec3( h, -h, -h), color },
            // Right face (+X)
            { Vec3( h, -h,  h), color }, { Vec3( h,  h,  h), color }, { Vec3( h,  h, -h), color }, { Vec3( h, -h, -h), color },
            // Left face (-X)
            { Vec3(-h, -h, -h), color }, { Vec3(-h,  h, -h), color }, { Vec3(-h,  h,  h), color }, { Vec3(-h, -h,  h), color }
        };

        const uint16_t indices[36] = {
            0,  1,  2,  0,  2,  3,  // Front
            4,  5,  6,  4,  6,  7,  // Back
            8,  9, 10,  8, 10, 11,  // Top
            12, 13, 14, 12, 14, 15, // Bottom
            16, 17, 18, 16, 18, 19, // Right
            20, 21, 22, 20, 22, 23  // Left
        };

        mesh->Initialise(device, vertices, indices);
        return mesh;
    }
}

