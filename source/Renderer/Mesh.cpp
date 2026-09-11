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

    std::shared_ptr<Mesh> Mesh::CreateCone(
        ID3D12Device* device,
        float radius,
        float height,
        uint32_t segments,
        const Vec4& color
    )
    {
        auto mesh = std::make_shared<Mesh>();

        if (segments < 3)
        {
            segments = 3;
        }

        const float halfH      = height * 0.5f;
        const float slantAngle = std::atan2(radius, height);
        const float cosSlant   = std::cos(slantAngle);
        const float sinSlant   = std::sin(slantAngle);

        std::vector<Vertex> vertices;
        std::vector<uint16_t> indices;

        // 1. Base cap (at y = -halfH) with normal pointing downward (0, -1, 0)
        const uint16_t baseCenterIdx = static_cast<uint16_t>(vertices.size());
        vertices.push_back({ Vec3(0.0f, -halfH, 0.0f), Vec3(0.0f, -1.0f, 0.0f), color });

        const uint16_t baseRimStart = static_cast<uint16_t>(vertices.size());
        for (uint32_t i = 0; i < segments; ++i)
        {
            const float theta = (Maths::TwoPi<float> * static_cast<float>(i)) / static_cast<float>(segments);
            const float x     = radius * std::cos(theta);
            const float z     = radius * std::sin(theta);
            vertices.push_back({ Vec3(x, -halfH, z), Vec3(0.0f, -1.0f, 0.0f), color });
        }

        for (uint32_t i = 0; i < segments; ++i)
        {
            const uint16_t curr = static_cast<uint16_t>(baseRimStart + i);
            const uint16_t next = static_cast<uint16_t>(baseRimStart + ((i + 1) % segments));
            // Outward downward (-Y) winding: center, curr, next
            indices.push_back(baseCenterIdx);
            indices.push_back(curr);
            indices.push_back(next);
        }

        // 2. Lateral cone surface (mantle) with outward slant normals
        for (uint32_t i = 0; i < segments; ++i)
        {
            const float theta0   = (Maths::TwoPi<float> * static_cast<float>(i)) / static_cast<float>(segments);
            const float theta1   = (Maths::TwoPi<float> * static_cast<float>(i + 1)) / static_cast<float>(segments);
            const float thetaMid = 0.5f * (theta0 + theta1);

            const Vec3 p0(radius * std::cos(theta0), -halfH, radius * std::sin(theta0));
            const Vec3 p1(radius * std::cos(theta1), -halfH, radius * std::sin(theta1));
            const Vec3 apex(0.0f, halfH, 0.0f);

            const Vec3 n0(std::cos(theta0) * cosSlant, sinSlant, std::sin(theta0) * cosSlant);
            const Vec3 n1(std::cos(theta1) * cosSlant, sinSlant, std::sin(theta1) * cosSlant);
            const Vec3 nApex(std::cos(thetaMid) * cosSlant, sinSlant, std::sin(thetaMid) * cosSlant);

            const uint16_t triStart = static_cast<uint16_t>(vertices.size());
            vertices.push_back({ apex, nApex.Normalised(), color });
            vertices.push_back({ p0,   n0.Normalised(),   color });
            vertices.push_back({ p1,   n1.Normalised(),   color });

            // Clockwise winding facing outward: apex, p1, p0
            indices.push_back(triStart);
            indices.push_back(triStart + 2);
            indices.push_back(triStart + 1);
        }

        mesh->Initialise(device, vertices, indices);
        return mesh;
    }

    std::shared_ptr<Mesh> Mesh::CreateSphere(
        ID3D12Device* device,
        float radius,
        uint32_t sliceCount,
        uint32_t stackCount,
        const Vec4& color
    )
    {
        auto mesh = std::make_shared<Mesh>();

        if (sliceCount < 3)
        {
            sliceCount = 3;
        }
        if (stackCount < 2)
        {
            stackCount = 2;
        }

        std::vector<Vertex> vertices;
        std::vector<uint16_t> indices;

        // 1. Top pole vertex (normal points straight up +Y)
        const uint16_t topPoleIdx = static_cast<uint16_t>(vertices.size());
        vertices.push_back({ Vec3(0.0f, radius, 0.0f), Vec3(0.0f, 1.0f, 0.0f), color });

        // 2. Intermediate stack rings (excluding top and bottom poles)
        const float phiStep   = Maths::Pi<float> / static_cast<float>(stackCount);
        const float thetaStep = Maths::TwoPi<float> / static_cast<float>(sliceCount);

        for (uint32_t i = 1; i < stackCount; ++i)
        {
            const float phi    = static_cast<float>(i) * phiStep;
            const float sinPhi = std::sin(phi);
            const float cosPhi = std::cos(phi);

            for (uint32_t j = 0; j < sliceCount; ++j)
            {
                const float theta    = static_cast<float>(j) * thetaStep;
                const float sinTheta = std::sin(theta);
                const float cosTheta = std::cos(theta);

                const Vec3 normal(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta);
                const Vec3 position = normal * radius;

                vertices.push_back({ position, normal.Normalised(), color });
            }
        }

        // 3. Bottom pole vertex (normal points straight down -Y)
        const uint16_t bottomPoleIdx = static_cast<uint16_t>(vertices.size());
        vertices.push_back({ Vec3(0.0f, -radius, 0.0f), Vec3(0.0f, -1.0f, 0.0f), color });

        // 4. Indices for top cap (connecting top pole to first ring)
        for (uint32_t j = 0; j < sliceCount; ++j)
        {
            const uint16_t curr = static_cast<uint16_t>(1 + j);
            const uint16_t next = static_cast<uint16_t>(1 + ((j + 1) % sliceCount));

            // Clockwise winding facing outward: topPole, next, curr
            indices.push_back(topPoleIdx);
            indices.push_back(next);
            indices.push_back(curr);
        }

        // 5. Indices for intermediate quads between rings
        for (uint32_t i = 0; i < stackCount - 2; ++i)
        {
            const uint16_t rowA = static_cast<uint16_t>(1 + i * sliceCount);
            const uint16_t rowB = static_cast<uint16_t>(1 + (i + 1) * sliceCount);

            for (uint32_t j = 0; j < sliceCount; ++j)
            {
                const uint16_t nextJ = static_cast<uint16_t>((j + 1) % sliceCount);

                const uint16_t a = rowA + static_cast<uint16_t>(j);
                const uint16_t b = rowA + nextJ;
                const uint16_t c = rowB + static_cast<uint16_t>(j);
                const uint16_t d = rowB + nextJ;

                // Triangle 1: a -> b -> d
                indices.push_back(a);
                indices.push_back(b);
                indices.push_back(d);

                // Triangle 2: a -> d -> c
                indices.push_back(a);
                indices.push_back(d);
                indices.push_back(c);
            }
        }

        // 6. Indices for bottom cap (connecting last ring to bottom pole)
        const uint16_t lastRowStart = static_cast<uint16_t>(1 + (stackCount - 2) * sliceCount);
        for (uint32_t j = 0; j < sliceCount; ++j)
        {
            const uint16_t curr = static_cast<uint16_t>(lastRowStart + j);
            const uint16_t next = static_cast<uint16_t>(lastRowStart + ((j + 1) % sliceCount));

            // Clockwise winding facing outward: curr, next, bottomPole
            indices.push_back(curr);
            indices.push_back(next);
            indices.push_back(bottomPoleIdx);
        }

        mesh->Initialise(device, vertices, indices);
        return mesh;
    }

    namespace
    {
        void AddSolidBox(
            std::vector<Vertex>& vertices,
            std::vector<uint16_t>& indices,
            const Vec3& min,
            const Vec3& max,
            const Vec4& color
        )
        {
            const uint16_t baseIndex = static_cast<uint16_t>(vertices.size());

            const Vec3 normalFront( 0.0f,  0.0f,  1.0f);
            const Vec3 normalBack ( 0.0f,  0.0f, -1.0f);
            const Vec3 normalTop  ( 0.0f,  1.0f,  0.0f);
            const Vec3 normalBottom(0.0f, -1.0f,  0.0f);
            const Vec3 normalRight( 1.0f,  0.0f,  0.0f);
            const Vec3 normalLeft (-1.0f,  0.0f,  0.0f);

            // 24 vertices (4 per face) with outward-facing surface normals
            const Vertex boxVertices[24] = {
                // Front face (+Z)
                { Vec3(min.x, min.y, max.z), normalFront, color },
                { Vec3(min.x, max.y, max.z), normalFront, color },
                { Vec3(max.x, max.y, max.z), normalFront, color },
                { Vec3(max.x, min.y, max.z), normalFront, color },
                // Back face (-Z)
                { Vec3(max.x, min.y, min.z), normalBack,  color },
                { Vec3(max.x, max.y, min.z), normalBack,  color },
                { Vec3(min.x, max.y, min.z), normalBack,  color },
                { Vec3(min.x, min.y, min.z), normalBack,  color },
                // Top face (+Y)
                { Vec3(min.x, max.y, max.z), normalTop,   color },
                { Vec3(min.x, max.y, min.z), normalTop,   color },
                { Vec3(max.x, max.y, min.z), normalTop,   color },
                { Vec3(max.x, max.y, max.z), normalTop,   color },
                // Bottom face (-Y)
                { Vec3(min.x, min.y, min.z), normalBottom,color },
                { Vec3(min.x, min.y, max.z), normalBottom,color },
                { Vec3(max.x, min.y, max.z), normalBottom,color },
                { Vec3(max.x, min.y, min.z), normalBottom,color },
                // Right face (+X)
                { Vec3(max.x, min.y, max.z), normalRight, color },
                { Vec3(max.x, max.y, max.z), normalRight, color },
                { Vec3(max.x, max.y, min.z), normalRight, color },
                { Vec3(max.x, min.y, min.z), normalRight, color },
                // Left face (-X)
                { Vec3(min.x, min.y, min.z), normalLeft,  color },
                { Vec3(min.x, max.y, min.z), normalLeft,  color },
                { Vec3(min.x, max.y, max.z), normalLeft,  color },
                { Vec3(min.x, min.y, max.z), normalLeft,  color }
            };

            for (const auto& v : boxVertices)
            {
                vertices.push_back(v);
            }

            // Outward-facing clockwise winding order for DirectX Left-Handed screen space
            const uint16_t boxIndices[36] = {
                0,  2,  1,  0,  3,  2,  // Front (+Z)
                4,  6,  5,  4,  7,  6,  // Back (-Z)
                8, 10,  9,  8, 11, 10,  // Top (+Y)
                12, 14, 13, 12, 15, 14, // Bottom (-Y)
                16, 18, 17, 16, 19, 18, // Right (+X)
                20, 22, 21, 20, 23, 22  // Left (-X)
            };

            for (uint16_t idx : boxIndices)
            {
                indices.push_back(baseIndex + idx);
            }
        }
    }

    std::shared_ptr<Mesh> Mesh::CreateCoordinateAxes(
        ID3D12Device* device,
        float shaftLength,
        float shaftRadius,
        float tipLength,
        float tipRadius
    )
    {
        auto mesh = std::make_shared<Mesh>();

        std::vector<Vertex> vertices;
        std::vector<uint16_t> indices;
        vertices.reserve(168);
        indices.reserve(252);

        // 1. Origin hub (neutral light grey)
        const float hubR = shaftRadius * 1.25f;
        AddSolidBox(vertices, indices, Vec3(-hubR, -hubR, -hubR), Vec3(hubR, hubR, hubR), Vec4(0.75f, 0.75f, 0.75f, 1.0f));

        // 2. Positive X Axis (Red)
        const Vec4 red = Vec4::Red();
        AddSolidBox(vertices, indices, Vec3(hubR, -shaftRadius, -shaftRadius), Vec3(shaftLength, shaftRadius, shaftRadius), red);
        AddSolidBox(vertices, indices, Vec3(shaftLength, -tipRadius, -tipRadius), Vec3(shaftLength + tipLength, tipRadius, tipRadius), red);

        // 3. Positive Y Axis (Green)
        const Vec4 green = Vec4::Green();
        AddSolidBox(vertices, indices, Vec3(-shaftRadius, hubR, -shaftRadius), Vec3(shaftRadius, shaftLength, shaftRadius), green);
        AddSolidBox(vertices, indices, Vec3(-tipRadius, shaftLength, -tipRadius), Vec3(tipRadius, shaftLength + tipLength, tipRadius), green);

        // 4. Positive Z Axis (Blue)
        const Vec4 blue = Vec4::Blue();
        AddSolidBox(vertices, indices, Vec3(-shaftRadius, -shaftRadius, hubR), Vec3(shaftRadius, shaftRadius, shaftLength), blue);
        AddSolidBox(vertices, indices, Vec3(-tipRadius, -tipRadius, shaftLength), Vec3(tipRadius, tipRadius, shaftLength + tipLength), blue);

        mesh->Initialise(device, vertices, indices);
        return mesh;
    }
}


