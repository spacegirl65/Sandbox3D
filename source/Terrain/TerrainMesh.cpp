// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "TerrainMesh.h"

#include <algorithm>
#include <thread>
#include <vector>

namespace Sandbox3D::Terrain
{
    using Renderer::Vertex;

    std::shared_ptr<Renderer::Mesh> TerrainMesh::Build(
        ID3D12Device* device,
        const TerrainGenerator& generator,
        double width,
        double depth,
        uint32_t resolutionX,
        uint32_t resolutionZ,
        const Maths::Vec3D& offset
    )
    {
        if (!device || resolutionX < 2 || resolutionZ < 2)
        {
            return nullptr;
        }

        const uint32_t totalVertices = resolutionX * resolutionZ;
        const uint32_t totalQuads    = (resolutionX - 1) * (resolutionZ - 1);
        const uint32_t totalIndices  = totalQuads * 6;

        std::vector<Vertex> vertices(totalVertices);

        const double stepX = width / static_cast<double>(resolutionX - 1);
        const double stepZ = depth / static_cast<double>(resolutionZ - 1);
        const double halfW = width * 0.5;
        const double halfD = depth * 0.5;
        const double normalStep = std::max(stepX, stepZ) * 0.5;

        // 1. Parallel vertex generation across CPU worker threads
        const unsigned int hardwareThreads = std::thread::hardware_concurrency();
        const unsigned int numThreads = std::max(1u, hardwareThreads == 0 ? 4u : hardwareThreads);
        const uint32_t rowsPerThread = (resolutionZ + numThreads - 1) / numThreads;

        std::vector<std::jthread> workers;
        workers.reserve(numThreads);

        for (unsigned int t = 0; t < numThreads; ++t)
        {
            const uint32_t zStart = t * rowsPerThread;
            const uint32_t zEnd   = std::min(zStart + rowsPerThread, resolutionZ);
            if (zStart >= zEnd)
            {
                break;
            }

            workers.emplace_back([&, zStart, zEnd]() {
                for (uint32_t iz = zStart; iz < zEnd; ++iz)
                {
                    const double lz = -halfD + static_cast<double>(iz) * stepZ;
                    const double wz = offset.z + lz;
                    const uint32_t rowOffset = iz * resolutionX;

                    for (uint32_t ix = 0; ix < resolutionX; ++ix)
                    {
                        const double lx = -halfW + static_cast<double>(ix) * stepX;
                        const double wx = offset.x + lx;

                        const double wy = generator.GenerateHeight(wx, wz);
                        const double ly = wy - offset.y;

                        const Vec3 normal = generator.CalculateNormal(wx, wz, normalStep);
                        const Vec4 color  = generator.EvaluateColor(wx, wy, wz, normal);

                        Vertex& vertex = vertices[rowOffset + ix];
                        vertex.position = Vec3(static_cast<float>(lx), static_cast<float>(ly), static_cast<float>(lz));
                        vertex.normal   = normal;
                        vertex.color    = color;
                    }
                }
            });
        }
        workers.clear(); // Explicitly join all worker threads before index building

        // 2. Generate 32-bit indices for the two triangles forming each quad
        std::vector<uint32_t> indices;
        indices.reserve(totalIndices);

        for (uint32_t iz = 0; iz < resolutionZ - 1; ++iz)
        {
            const uint32_t row0 = iz * resolutionX;
            const uint32_t row1 = (iz + 1) * resolutionX;

            for (uint32_t ix = 0; ix < resolutionX - 1; ++ix)
            {
                const uint32_t i0 = row0 + ix;     // Bottom-Left
                const uint32_t i1 = row0 + ix + 1; // Bottom-Right
                const uint32_t i2 = row1 + ix;     // Top-Left
                const uint32_t i3 = row1 + ix + 1; // Top-Right

                // Triangle 1: i0 -> i2 -> i3 (Clockwise in Left-Handed space)
                indices.push_back(i0);
                indices.push_back(i2);
                indices.push_back(i3);

                // Triangle 2: i0 -> i3 -> i1 (Clockwise in Left-Handed space)
                indices.push_back(i0);
                indices.push_back(i3);
                indices.push_back(i1);
            }
        }

        auto mesh = std::make_shared<Renderer::Mesh>();
        mesh->Initialise(device, vertices, indices);
        return mesh;
    }
}

