// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "TerrainGenerator.h"
#include "Renderer/Mesh.h"

#include <d3d12.h>
#include <memory>

namespace Sandbox3D::Engine
{
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

    // Constructs Direct3D 12 indexed geometry meshes from continuous terrain generators or elevation datasets
    class TerrainMesh final
    {
    public:
        TerrainMesh() = default;
        ~TerrainMesh() = default;

        // Builds an indexed mesh covering the specified bounds sampled at the given resolution
        [[nodiscard]] static std::shared_ptr<Renderer::Mesh> Build(
            ID3D12Device* device,
            const TerrainGenerator& generator,
            double width,
            double depth,
            uint32_t resolutionX,
            uint32_t resolutionZ,
            const Maths::Vec3D& offset = Maths::Vec3D(0.0, 0.0, 0.0)
        );

        // Builds an indexed mesh from raw elevation samples (e.g. from LiDAR DTM tiles)
        [[nodiscard]] static std::shared_ptr<Renderer::Mesh> BuildFromHeightmap(
            ID3D12Device* device,
            std::span<const float> elevations,
            uint32_t resolutionX,
            uint32_t resolutionZ,
            double width,
            double depth,
            const Maths::Vec3D& offset = Maths::Vec3D(0.0, 0.0, 0.0)
        );

        // Loads a binary DTM heightmap file (.bin) and builds the mesh
        [[nodiscard]] static std::shared_ptr<Renderer::Mesh> BuildFromFile(
            ID3D12Device* device,
            std::string_view filePath,
            const Maths::Vec3D& offset = Maths::Vec3D(0.0, 0.0, 0.0)
        );
    };
}

