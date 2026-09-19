// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Maths/Maths.h"

#include <d3d12.h>
#include <memory>
#include <span>
#include <vector>

namespace Sandbox3D::Renderer
{
    using Maths::Vec3;
    using Maths::Vec4;
    using Maths::BoundingBox;
    using Maths::BoundingSphere;

#pragma pack(push, 1)
    // Compiled binary mesh header (.mesh) layout for direct GPU streaming
    struct MeshFileHeader
    {
        char     magic[4]{ 'S', '3', 'D', 'M' };
        uint32_t version{ 1 };
        uint32_t vertexCount{ 0 };
        uint32_t indexCount{ 0 };
        float    minX{ 0.0f };
        float    minY{ 0.0f };
        float    minZ{ 0.0f };
        float    maxX{ 0.0f };
        float    maxY{ 0.0f };
        float    maxZ{ 0.0f };
        double   originX{ 0.0 };
        double   originZ{ 0.0 };
        double   width{ 0.0 };
        double   depth{ 0.0 };
        float    minElevation{ 0.0f };
        float    maxElevation{ 0.0f };
    };
#pragma pack(pop)

    // Generalised geometry mesh encapsulating arbitrary vertex and index buffers with spatial bounding volumes
    class Mesh final
    {
    public:
        Mesh() = default;
        ~Mesh() = default;

        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;
        Mesh(Mesh&&) noexcept = default;
        Mesh& operator=(Mesh&&) noexcept = default;

        // Initialise with non-indexed vertices (draw via DrawInstanced)
        void Initialise(ID3D12Device* device, std::span<const Vertex> vertices);

        // Initialise with 16-bit indexed vertices (draw via DrawIndexedInstanced)
        void Initialise(ID3D12Device* device, std::span<const Vertex> vertices, std::span<const uint16_t> indices);

        // Initialise with 32-bit indexed vertices (draw via DrawIndexedInstanced)
        void Initialise(ID3D12Device* device, std::span<const Vertex> vertices, std::span<const uint32_t> indices);

        // Loads a compiled binary mesh (.mesh) file directly into GPU memory
        [[nodiscard]] static std::shared_ptr<Mesh> LoadFromFile(
            ID3D12Device* device,
            std::string_view filePath,
            MeshFileHeader* outHeader = nullptr
        );

        // Binds geometry buffers and issues draw call
        void Draw(ID3D12GraphicsCommandList* commandList) const noexcept;

        [[nodiscard]] bool IsInitialised() const noexcept { return m_vertexBuffer.GetVertexCount() > 0; }
        [[nodiscard]] bool IsIndexed() const noexcept { return m_isIndexed; }
        [[nodiscard]] const VertexBuffer& GetVertexBuffer() const noexcept { return m_vertexBuffer; }
        [[nodiscard]] const IndexBuffer& GetIndexBuffer() const noexcept { return m_indexBuffer; }
        [[nodiscard]] const BoundingBox& GetBoundingBox() const noexcept { return m_boundingBox; }
        [[nodiscard]] const BoundingSphere& GetBoundingSphere() const noexcept { return m_boundingSphere; }
        [[nodiscard]] size_t GetTriangleCount() const noexcept
        {
            if (m_isIndexed)
            {
                return static_cast<size_t>(m_indexBuffer.GetIndexCount()) / 3;
            }
            return static_cast<size_t>(m_vertexBuffer.GetVertexCount()) / 3;
        }
        [[nodiscard]] size_t GetVertexCount() const noexcept { return m_vertexBuffer.GetVertexCount(); }

        // --- Standard Geometric Mesh Factories ---

        // Origin-centred quad [-0.5, 0.5] with two distinct solid triangles (Red and Blue)
        [[nodiscard]] static std::shared_ptr<Mesh> CreateRedAndBlueQuad(ID3D12Device* device);

        // General planar quad with 4 corner vertices and 6 indices
        [[nodiscard]] static std::shared_ptr<Mesh> CreateQuad(
            ID3D12Device* device,
            float width = 1.0f,
            float height = 1.0f,
            const Vec4& color = Vec4::White()
        );

        // 3D Box / Cube mesh centred at the origin
        [[nodiscard]] static std::shared_ptr<Mesh> CreateCube(
            ID3D12Device* device,
            float size = 1.0f,
            const Vec4& color = Vec4::White()
        );

        // 3D Cone mesh centred vertically at the origin [-height/2, height/2]
        [[nodiscard]] static std::shared_ptr<Mesh> CreateCone(
            ID3D12Device* device,
            float radius = 0.5f,
            float height = 1.0f,
            uint32_t segments = 36,
            const Vec4& color = Vec4::White()
        );

        // 3D Sphere mesh centred at the origin
        [[nodiscard]] static std::shared_ptr<Mesh> CreateSphere(
            ID3D12Device* device,
            float radius = 0.5f,
            uint32_t sliceCount = 36,
            uint32_t stackCount = 18,
            const Vec4& color = Vec4::White()
        );

        // 3D Coordinate Frame Axes (X = Red, Y = Green, Z = Blue) with solid shafts and arrowhead tips
        [[nodiscard]] static std::shared_ptr<Mesh> CreateCoordinateAxes(
            ID3D12Device* device,
            float shaftLength = 0.68f,
            float shaftRadius = 0.032f,
            float tipLength = 0.20f,
            float tipRadius = 0.075f
        );

        // 3D Wireframe Box mesh centred at the origin with solid edge shafts
        [[nodiscard]] static std::shared_ptr<Mesh> CreateWireframeBox(
            ID3D12Device* device,
            float size = 1.0f,
            float lineWidth = 0.003f,
            const Vec4& color = Vec4::White()
        );

    private:
        VertexBuffer   m_vertexBuffer;
        IndexBuffer    m_indexBuffer;
        BoundingBox    m_boundingBox{};
        BoundingSphere m_boundingSphere{};
        bool           m_isIndexed{ false };
    };
}

