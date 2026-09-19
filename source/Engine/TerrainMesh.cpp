// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "TerrainMesh.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <thread>
#include <vector>

namespace Sandbox3D::Engine
{
    using Renderer::Vertex;

    TerrainMeshData TerrainMesh::Generate(
        const TerrainGenerator& generator,
        double width,
        double depth,
        uint32_t resolutionX,
        uint32_t resolutionZ,
        const Maths::Vec3D& offset
    )
    {
        if (resolutionX < 2 || resolutionZ < 2)
        {
            return {};
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

        // Parallel vertex generation across CPU worker threads
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

        // Generate 32-bit indices for the two triangles forming each quad
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

        return TerrainMeshData{
            .vertices = std::move(vertices),
            .indices  = std::move(indices)
        };
    }

    TerrainMeshData TerrainMesh::GenerateFromHeightmap(
        std::span<const float> elevations,
        uint32_t resolutionX,
        uint32_t resolutionZ,
        double width,
        double depth,
        const Maths::Vec3D& offset
    )
    {
        if (resolutionX < 2 || resolutionZ < 2 || elevations.size() < static_cast<size_t>(resolutionX) * resolutionZ)
        {
            return {};
        }

        const uint32_t totalVertices = resolutionX * resolutionZ;
        const uint32_t totalQuads    = (resolutionX - 1) * (resolutionZ - 1);
        const uint32_t totalIndices  = totalQuads * 6;

        std::vector<Vertex> vertices(totalVertices);

        const double stepX = width / static_cast<double>(resolutionX - 1);
        const double stepZ = depth / static_cast<double>(resolutionZ - 1);
        const double halfW = width * 0.5;
        const double halfD = depth * 0.5;

        // Sample elevation at grid cell index (clamped to border)
        auto sampleElevation = [&](int32_t gx, int32_t gz) -> float {
            const int32_t cx = std::clamp(gx, 0, static_cast<int32_t>(resolutionX - 1));
            const int32_t cz = std::clamp(gz, 0, static_cast<int32_t>(resolutionZ - 1));
            return elevations[cz * resolutionX + cx];
        };

        // Parallel vertex generation across CPU worker threads
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
                    const uint32_t rowOffset = iz * resolutionX;

                    for (uint32_t ix = 0; ix < resolutionX; ++ix)
                    {
                        const double lx = -halfW + static_cast<double>(ix) * stepX;

                        const float hC = sampleElevation(static_cast<int32_t>(ix), static_cast<int32_t>(iz));
                        const double wy = static_cast<double>(hC);
                        const double ly = wy - offset.y;

                        // Central difference normal calculation from neighbouring height samples
                        const float hL = sampleElevation(static_cast<int32_t>(ix) - 1, static_cast<int32_t>(iz));
                        const float hR = sampleElevation(static_cast<int32_t>(ix) + 1, static_cast<int32_t>(iz));
                        const float hD = sampleElevation(static_cast<int32_t>(ix), static_cast<int32_t>(iz) - 1);
                        const float hU = sampleElevation(static_cast<int32_t>(ix), static_cast<int32_t>(iz) + 1);

                        const float dx = (hR - hL) / static_cast<float>(2.0 * stepX);
                        const float dz = (hU - hD) / static_cast<float>(2.0 * stepZ);
                        const Vec3 normal = Vec3(-dx, 1.0f, -dz).Normalised();

                        // British landscape palette: elevation and slope tinting
                        const float slopeFactor = std::clamp(1.0f - normal.y, 0.0f, 1.0f);
                        const float heightRatio = std::clamp(static_cast<float>((wy - 100.0) / 400.0), 0.0f, 1.0f);

                        Vec4 color;
                        if (slopeFactor > 0.45f)
                        {
                            // Steep exposed limestone/gritstone scar
                            color = Vec4(0.48f, 0.46f, 0.44f, 1.0f);
                        }
                        else if (heightRatio > 0.65f)
                        {
                            // High moorland peat, heather, and bent-grass
                            color = Vec4(0.38f, 0.35f, 0.26f, 1.0f);
                        }
                        else
                        {
                            // Lush upland valley pasture
                            color = Vec4(0.28f, 0.42f, 0.22f, 1.0f);
                        }

                        Vertex& vertex = vertices[rowOffset + ix];
                        vertex.position = Vec3(static_cast<float>(lx), static_cast<float>(ly), static_cast<float>(lz));
                        vertex.normal   = normal;
                        vertex.color    = color;
                    }
                }
            });
        }
        workers.clear(); // Explicitly join all worker threads before index building

        // Generate 32-bit indices for the two triangles forming each quad
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

        return TerrainMeshData{
            .vertices = std::move(vertices),
            .indices  = std::move(indices)
        };
    }

    TerrainMeshData TerrainMesh::GenerateFromFile(
        std::string_view filePath,
        const Maths::Vec3D& offset,
        MeshFileHeader* outHeader
    )
    {
        std::ifstream file(std::string(filePath), std::ios::binary);
        if (!file.is_open())
        {
            return {};
        }

        char magic[4] = {};
        file.read(magic, 4);
        if (!file)
        {
            return {};
        }
        file.seekg(0, std::ios::beg);

        // Check if file is a compiled binary mesh (.mesh)
        if (magic[0] == 'S' && magic[1] == '3' && magic[2] == 'D' && magic[3] == 'M')
        {
            MeshFileHeader header{};
            file.read(reinterpret_cast<char*>(&header), sizeof(header));
            if (!file || header.vertexCount == 0)
            {
                return {};
            }

            if (outHeader)
            {
                *outHeader = header;
            }

            std::vector<Vertex> vertices(header.vertexCount);
            file.read(reinterpret_cast<char*>(vertices.data()), static_cast<std::streamsize>(header.vertexCount * sizeof(Vertex)));
            if (!file)
            {
                return {};
            }

            std::vector<uint32_t> indices;
            if (header.indexCount > 0)
            {
                indices.resize(header.indexCount);
                file.read(reinterpret_cast<char*>(indices.data()), static_cast<std::streamsize>(header.indexCount * sizeof(uint32_t)));
                if (!file)
                {
                    return {};
                }
            }

            // Adjust vertices by offset if an offset is provided
            if (offset.x != 0.0 || offset.y != 0.0 || offset.z != 0.0)
            {
                const Vec3 offsetF(static_cast<float>(offset.x), static_cast<float>(offset.y), static_cast<float>(offset.z));
                for (auto& v : vertices)
                {
                    v.position = v.position - offsetF;
                }
            }

            return TerrainMeshData{
                .vertices = std::move(vertices),
                .indices  = std::move(indices)
            };
        }

        // Check if file is a binary DTM heightmap (.bin)
        if (magic[0] == 'D' && magic[1] == 'T' && magic[2] == 'M' && magic[3] == '1')
        {
            DtmHeightmapHeader header{};
            file.read(reinterpret_cast<char*>(&header), sizeof(header));
            if (header.resolutionX < 2 || header.resolutionZ < 2)
            {
                return {};
            }

            if (outHeader)
            {
                outHeader->magic[0] = 'S';
                outHeader->magic[1] = '3';
                outHeader->magic[2] = 'D';
                outHeader->magic[3] = 'M';
                outHeader->version  = 1;
                outHeader->originX  = header.originX;
                outHeader->originZ  = header.originZ;
                outHeader->width    = header.width;
                outHeader->depth    = header.depth;
                outHeader->minElevation = header.minElevation;
                outHeader->maxElevation = header.maxElevation;
            }

            const size_t totalSamples = static_cast<size_t>(header.resolutionX) * header.resolutionZ;
            std::vector<float> elevations(totalSamples);
            file.read(reinterpret_cast<char*>(elevations.data()), static_cast<std::streamsize>(totalSamples * sizeof(float)));
            if (!file)
            {
                return {};
            }

            return GenerateFromHeightmap(
                elevations,
                header.resolutionX,
                header.resolutionZ,
                header.width,
                header.depth,
                offset
            );
        }

        return {};
    }

    void TerrainMesh::ApplyProceduralPalette(
        std::span<Vertex> vertices,
        const TerrainConfig& config,
        float minElevation,
        float maxElevation
    )
    {
        if (vertices.empty())
        {
            return;
        }

        const float elevSpan = std::max(maxElevation - minElevation, 1.0f);
        const unsigned int hardwareThreads = std::thread::hardware_concurrency();
        const unsigned int numThreads = std::max(1u, hardwareThreads == 0 ? 4u : hardwareThreads);
        const size_t totalVertices = vertices.size();
        const size_t chunkSize = (totalVertices + numThreads - 1) / numThreads;

        std::vector<std::jthread> workers;
        workers.reserve(numThreads);

        for (unsigned int t = 0; t < numThreads; ++t)
        {
            const size_t start = t * chunkSize;
            const size_t end   = std::min(start + chunkSize, totalVertices);
            if (start >= end)
            {
                break;
            }

            workers.emplace_back([&, start, end]() {
                Maths::Noise noise(42);

                for (size_t i = start; i < end; ++i)
                {
                    auto& vertex = vertices[i];
                    const float elev = vertex.position.y;
                    const float altNorm = std::clamp((elev - minElevation) / elevSpan, 0.0f, 1.0f);
                    const float slope = 1.0f - std::clamp(vertex.normal.y, 0.0f, 1.0f);

                    // Multi-scale organic Perlin noise mottling
                    const float mottling = noise.Perlin(vertex.position.x * 0.012f, vertex.position.z * 0.012f) * 0.025f +
                                           noise.Perlin(vertex.position.x * 0.045f, vertex.position.z * 0.045f) * 0.015f;

                    // Altitudinal vegetation belts matching British upland ecology
                    Maths::Vec4 baseVegColor;
                    if (altNorm < 0.22f)
                    {
                        const float factor = altNorm / 0.22f;
                        baseVegColor = config.valleyFloorColor.Lerp(config.lowSlopeColor, factor);
                    }
                    else if (altNorm < 0.55f)
                    {
                        const float factor = (altNorm - 0.22f) / 0.33f;
                        baseVegColor = config.lowSlopeColor.Lerp(config.midSlopeColor, factor);
                    }
                    else if (altNorm < 0.78f)
                    {
                        const float factor = (altNorm - 0.55f) / 0.23f;
                        baseVegColor = config.midSlopeColor.Lerp(config.highPlateauColor, factor);
                    }
                    else
                    {
                        const float factor = std::clamp((altNorm - 0.78f) / 0.22f, 0.0f, 1.0f);
                        baseVegColor = config.highPlateauColor.Lerp(config.peatMoorColor, factor);
                    }

                    // Stepped cyclothem limestone scars and sheer crags with stratum banding
                    Maths::Vec4 finalColor;
                    if (slope > 0.085f)
                    {
                        const float factor = std::clamp((slope - 0.085f) / 0.15f, 0.0f, 1.0f);
                        const float cragBanding = std::sin(elev * 0.35f) * 0.030f;
                        Maths::Vec4 cragTone = config.rockColor.Lerp(config.steepCragColor, factor);
                        cragTone.x = std::clamp(cragTone.x + cragBanding, 0.0f, 1.0f);
                        cragTone.y = std::clamp(cragTone.y + cragBanding, 0.0f, 1.0f);
                        cragTone.z = std::clamp(cragTone.z + cragBanding, 0.0f, 1.0f);
                        finalColor = cragTone;
                    }
                    else if (slope > 0.035f)
                    {
                        const float factor = (slope - 0.035f) / 0.050f;
                        const float stratumBanding = std::sin(elev * 0.35f) * 0.035f;
                        Maths::Vec4 scarTone = config.limestoneScarColor.Lerp(config.rockColor, 0.35f);
                        scarTone.x = std::clamp(scarTone.x + stratumBanding, 0.0f, 1.0f);
                        scarTone.y = std::clamp(scarTone.y + stratumBanding, 0.0f, 1.0f);
                        scarTone.z = std::clamp(scarTone.z + stratumBanding, 0.0f, 1.0f);
                        finalColor = baseVegColor.Lerp(scarTone, factor);
                    }
                    else
                    {
                        finalColor = baseVegColor;
                    }

                    // Apply subtle organic luminance variation
                    finalColor.x = std::clamp(finalColor.x + mottling, 0.0f, 1.0f);
                    finalColor.y = std::clamp(finalColor.y + mottling, 0.0f, 1.0f);
                    finalColor.z = std::clamp(finalColor.z + mottling, 0.0f, 1.0f);
                    finalColor.w = 1.0f;

                    vertex.color = finalColor;
                }
            });
        }
    }
}

