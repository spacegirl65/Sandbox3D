// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "TerrainGenerator.h"
#include "Renderer/Mesh.h"

#include <d3d12.h>
#include <memory>

namespace Sandbox3D::Terrain
{
    // Constructs Direct3D 12 indexed geometry meshes from continuous terrain generators
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
    };
}

