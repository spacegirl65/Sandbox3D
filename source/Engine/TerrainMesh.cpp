// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "TerrainMesh.h"

#include <algorithm>
#include <fstream>
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

    std::shared_ptr<Renderer::Mesh> TerrainMesh::BuildFromHeightmap(
        ID3D12Device* device,
        std::span<const float> elevations,
        uint32_t resolutionX,
        uint32_t resolutionZ,
        double width,
        double depth,
        const Maths::Vec3D& offset
    )
    {
        if (!device || resolutionX < 2 || resolutionZ < 2 || elevations.size() < static_cast<size_t>(resolutionX) * resolutionZ)
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

        // Determine min and max elevation for colour shading
        float minH = 1e9f;
        float maxH = -1e9f;
        for (float h : elevations)
        {
            if (h > -999.0f)
            {
                minH = std::min(minH, h);
                maxH = std::max(maxH, h);
            }
        }
        if (minH >= maxH)
        {
            minH = 0.0f;
            maxH = 1000.0f;
        }
        const float rangeH = std::max(maxH - minH, 1.0f);

        auto sampleHeight = [&](uint32_t x, uint32_t z) -> float {
            x = std::clamp(x, 0u, resolutionX - 1);
            z = std::clamp(z, 0u, resolutionZ - 1);
            const float val = elevations[z * resolutionX + x];
            return (val < -999.0f) ? minH : val;
        };

        const Vec4 valleyColor(0.21f, 0.41f, 0.15f, 1.0f);   // Lush valley pasture
        const Vec4 midSlopeColor(0.44f, 0.42f, 0.24f, 1.0f); // Moorland grass
        const Vec4 rockColor(0.48f, 0.47f, 0.43f, 1.0f);     // Weathered gritstone / limestone
        const Vec4 plateauColor(0.35f, 0.30f, 0.24f, 1.0f);  // Peat summit plateau

        for (uint32_t iz = 0; iz < resolutionZ; ++iz)
        {
            const double lz = -halfD + static_cast<double>(iz) * stepZ;
            const uint32_t rowOffset = iz * resolutionX;

            for (uint32_t ix = 0; ix < resolutionX; ++ix)
            {
                const double lx = -halfW + static_cast<double>(ix) * stepX;
                const float h = sampleHeight(ix, iz);
                const double ly = static_cast<double>(h) - offset.y;

                // Finite difference normal
                const float hL = sampleHeight(ix > 0 ? ix - 1 : 0, iz);
                const float hR = sampleHeight(ix + 1 < resolutionX ? ix + 1 : resolutionX - 1, iz);
                const float hD = sampleHeight(ix, iz > 0 ? iz - 1 : 0);
                const float hU = sampleHeight(ix, iz + 1 < resolutionZ ? iz + 1 : resolutionZ - 1);

                const float dx = (hR - hL) / static_cast<float>(2.0 * stepX);
                const float dz = (hU - hD) / static_cast<float>(2.0 * stepZ);
                const Vec3 normal = Vec3(-dx, 1.0f, -dz).Normalised();

                // Biome colour interpolation based on elevation and slope
                const float normH = std::clamp((h - minH) / rangeH, 0.0f, 1.0f);
                const float slope = 1.0f - normal.y; // 0 = flat, 1 = vertical

                Vec4 color = valleyColor;
                if (normH < 0.35f)
                {
                    color = valleyColor.Lerp(midSlopeColor, normH / 0.35f);
                }
                else if (normH < 0.75f)
                {
                    color = midSlopeColor.Lerp(rockColor, (normH - 0.35f) / 0.4f);
                }
                else
                {
                    color = rockColor.Lerp(plateauColor, (normH - 0.75f) / 0.25f);
                }

                // Blend rock colour on steep slopes
                if (slope > 0.35f)
                {
                    const float rockWeight = std::clamp((slope - 0.35f) / 0.35f, 0.0f, 1.0f);
                    color = color.Lerp(rockColor, rockWeight);
                }

                Vertex& vertex = vertices[rowOffset + ix];
                vertex.position = Vec3(static_cast<float>(lx), static_cast<float>(ly), static_cast<float>(lz));
                vertex.normal   = normal;
                vertex.color    = color;
            }
        }

        // Generate 32-bit indices
        std::vector<uint32_t> indices;
        indices.reserve(totalIndices);

        for (uint32_t iz = 0; iz < resolutionZ - 1; ++iz)
        {
            const uint32_t row0 = iz * resolutionX;
            const uint32_t row1 = (iz + 1) * resolutionX;

            for (uint32_t ix = 0; ix < resolutionX - 1; ++ix)
            {
                const uint32_t i0 = row0 + ix;
                const uint32_t i1 = row0 + ix + 1;
                const uint32_t i2 = row1 + ix;
                const uint32_t i3 = row1 + ix + 1;

                indices.push_back(i0);
                indices.push_back(i2);
                indices.push_back(i3);

                indices.push_back(i0);
                indices.push_back(i3);
                indices.push_back(i1);
            }
        }

        auto mesh = std::make_shared<Renderer::Mesh>();
        mesh->Initialise(device, vertices, indices);
        return mesh;
    }

    std::shared_ptr<Renderer::Mesh> TerrainMesh::BuildFromFile(
        ID3D12Device* device,
        std::string_view filePath,
        const Maths::Vec3D& offset
    )
    {
        std::ifstream file(std::string(filePath), std::ios::binary);
        if (!file.is_open())
        {
            return nullptr;
        }

        DtmHeightmapHeader header{};
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (header.magic[0] != 'D' || header.magic[1] != 'T' || header.magic[2] != 'M' || header.magic[3] != '1')
        {
            return nullptr;
        }

        if (header.resolutionX < 2 || header.resolutionZ < 2)
        {
            return nullptr;
        }

        const size_t totalSamples = static_cast<size_t>(header.resolutionX) * header.resolutionZ;
        std::vector<float> elevations(totalSamples);
        file.read(reinterpret_cast<char*>(elevations.data()), static_cast<std::streamsize>(totalSamples * sizeof(float)));
        if (!file)
        {
            return nullptr;
        }

        return BuildFromHeightmap(
            device,
            elevations,
            header.resolutionX,
            header.resolutionZ,
            header.width,
            header.depth,
            offset
        );
    }
}

