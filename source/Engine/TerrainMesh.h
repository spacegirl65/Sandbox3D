// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "TerrainGenerator.h"
#include "Renderer/VertexBuffer.h"

#include <span>
#include <string_view>
#include <vector>

namespace Sandbox3D::Engine
{
    using Renderer::Vertex;

#pragma pack(push, 1)
    // Binary DTM heightmap header format for pre-processed LiDAR datasets
    struct DtmHeightmapHeader
    {
        char     magic[4]{ 'D', 'T', 'M', '1' };
        uint32_t resolutionX{ 0 };
        uint32_t resolutionZ{ 0 };
        double   originX{ 0.0 };
        double   originZ{ 0.0 };
        double   width{ 0.0 };
        double   depth{ 0.0 };
        float    minElevation{ 0.0f };
        float    maxElevation{ 0.0f };
    };
#pragma pack(pop)

    // CPU-side vertex and index buffers generated for terrain surfaces or cubic cells
    struct TerrainMeshData
    {
        std::vector<Vertex>   vertices;
        std::vector<uint32_t> indices;

        [[nodiscard]] bool IsEmpty() const noexcept { return vertices.empty() || indices.empty(); }
    };

    // Constructs indexed CPU terrain geometry from continuous procedural generators or elevation datasets
    class TerrainMesh final
    {
    public:
        TerrainMesh() = default;
        ~TerrainMesh() = default;

        // Generates an indexed mesh covering the specified bounds sampled at the given resolution
        [[nodiscard]] static TerrainMeshData Generate(
            const TerrainGenerator& generator,
            double width,
            double depth,
            uint32_t resolutionX,
            uint32_t resolutionZ,
            const Maths::Vec3D& offset = Maths::Vec3D(0.0, 0.0, 0.0)
        );

        // Generates an indexed mesh from raw elevation samples (e.g. from LiDAR DTM tiles)
        [[nodiscard]] static TerrainMeshData GenerateFromHeightmap(
            std::span<const float> elevations,
            uint32_t resolutionX,
            uint32_t resolutionZ,
            double width,
            double depth,
            const Maths::Vec3D& offset = Maths::Vec3D(0.0, 0.0, 0.0)
        );

        // Loads a binary DTM heightmap file (.bin) and generates the indexed CPU mesh
        [[nodiscard]] static TerrainMeshData GenerateFromFile(
            std::string_view filePath,
            const Maths::Vec3D& offset = Maths::Vec3D(0.0, 0.0, 0.0)
        );
    };
}

