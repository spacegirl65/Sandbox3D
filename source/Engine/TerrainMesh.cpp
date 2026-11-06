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

    std::vector<uint32_t> TerrainMesh::GenerateAlternatingIndices(
        uint32_t resolutionX,
        uint32_t resolutionZ
    )
    {
        if (resolutionX < 2 || resolutionZ < 2)
        {
            return {};
        }

        const uint32_t quadCountX = resolutionX - 1;
        const uint32_t quadCountZ = resolutionZ - 1;
        constexpr uint32_t indicesPerQuad = 6;
        const uint32_t totalIndices = quadCountX * quadCountZ * indicesPerQuad;

        std::vector<uint32_t> indices;
        indices.reserve(totalIndices);

        for (uint32_t iz = 0; iz < quadCountZ; ++iz)
        {
            const uint32_t row0 = iz * resolutionX;
            const uint32_t row1 = (iz + 1) * resolutionX;

            for (uint32_t ix = 0; ix < quadCountX; ++ix)
            {
                const uint32_t i0 = row0 + ix;     // Bottom-Left
                const uint32_t i1 = row0 + ix + 1; // Bottom-Right
                const uint32_t i2 = row1 + ix;     // Top-Left
                const uint32_t i3 = row1 + ix + 1; // Top-Right

                // Checkerboard alternating diagonals eliminate directional triangulation bias
                if ((ix + iz) % 2 == 0)
                {
                    // Primary diagonal from i0 to i3 (Bottom-Left to Top-Right)
                    // Triangle 1: i0 -> i2 -> i3 (Clockwise in Left-Handed space)
                    indices.push_back(i0);
                    indices.push_back(i2);
                    indices.push_back(i3);

                    // Triangle 2: i0 -> i3 -> i1 (Clockwise in Left-Handed space)
                    indices.push_back(i0);
                    indices.push_back(i3);
                    indices.push_back(i1);
                }
                else
                {
                    // Secondary diagonal from i1 to i2 (Bottom-Right to Top-Left)
                    // Triangle 1: i0 -> i2 -> i1 (Clockwise in Left-Handed space)
                    indices.push_back(i0);
                    indices.push_back(i2);
                    indices.push_back(i1);

                    // Triangle 2: i1 -> i2 -> i3 (Clockwise in Left-Handed space)
                    indices.push_back(i1);
                    indices.push_back(i2);
                    indices.push_back(i3);
                }
            }
        }

        return indices;
    }

    std::vector<uint32_t> TerrainMesh::GenerateTiledAlternatingIndices(
        std::span<const Vertex> vertices,
        uint32_t resolutionX,
        uint32_t resolutionZ,
        std::vector<Renderer::MeshCluster>& outClusters,
        uint32_t tileQuads
    )
    {
        outClusters.clear();
        if (resolutionX < 2 || resolutionZ < 2 || vertices.size() < static_cast<size_t>(resolutionX) * resolutionZ)
        {
            return {};
        }

        const uint32_t effectiveTileQuads = std::max(4u, tileQuads);
        const uint32_t quadCountX = resolutionX - 1;
        const uint32_t quadCountZ = resolutionZ - 1;
        constexpr uint32_t indicesPerQuad = 6;
        const uint32_t totalIndices = quadCountX * quadCountZ * indicesPerQuad;

        const uint32_t tilesX = (quadCountX + effectiveTileQuads - 1) / effectiveTileQuads;
        const uint32_t tilesZ = (quadCountZ + effectiveTileQuads - 1) / effectiveTileQuads;
        outClusters.reserve(static_cast<size_t>(tilesX) * tilesZ);

        std::vector<uint32_t> indices;
        indices.reserve(totalIndices);

        for (uint32_t tz = 0; tz < quadCountZ; tz += effectiveTileQuads)
        {
            const uint32_t zEnd = std::min(tz + effectiveTileQuads, quadCountZ);

            for (uint32_t tx = 0; tx < quadCountX; tx += effectiveTileQuads)
            {
                const uint32_t xEnd = std::min(tx + effectiveTileQuads, quadCountX);
                const uint32_t startIndex = static_cast<uint32_t>(indices.size());

                const Maths::Vec3 firstPos = vertices[static_cast<size_t>(tz) * resolutionX + tx].position;
                Maths::Vec3 minPos = firstPos;
                Maths::Vec3 maxPos = firstPos;

                for (uint32_t iz = tz; iz < zEnd; ++iz)
                {
                    const uint32_t row0 = iz * resolutionX;
                    const uint32_t row1 = (iz + 1) * resolutionX;

                    for (uint32_t ix = tx; ix < xEnd; ++ix)
                    {
                        const uint32_t i0 = row0 + ix;
                        const uint32_t i1 = row0 + ix + 1;
                        const uint32_t i2 = row1 + ix;
                        const uint32_t i3 = row1 + ix + 1;

                        minPos = Maths::Vec3::Min(minPos, vertices[i0].position);
                        minPos = Maths::Vec3::Min(minPos, vertices[i1].position);
                        minPos = Maths::Vec3::Min(minPos, vertices[i2].position);
                        minPos = Maths::Vec3::Min(minPos, vertices[i3].position);

                        maxPos = Maths::Vec3::Max(maxPos, vertices[i0].position);
                        maxPos = Maths::Vec3::Max(maxPos, vertices[i1].position);
                        maxPos = Maths::Vec3::Max(maxPos, vertices[i2].position);
                        maxPos = Maths::Vec3::Max(maxPos, vertices[i3].position);

                        if ((ix + iz) % 2 == 0)
                        {
                            indices.push_back(i0);
                            indices.push_back(i2);
                            indices.push_back(i3);

                            indices.push_back(i0);
                            indices.push_back(i3);
                            indices.push_back(i1);
                        }
                        else
                        {
                            indices.push_back(i0);
                            indices.push_back(i2);
                            indices.push_back(i1);

                            indices.push_back(i1);
                            indices.push_back(i2);
                            indices.push_back(i3);
                        }
                    }
                }

                const uint32_t indexCount = static_cast<uint32_t>(indices.size()) - startIndex;
                constexpr float halfScale = 0.5f;
                const Maths::Vec3 center = (minPos + maxPos) * halfScale;
                const float radius = (maxPos - center).Length();

                outClusters.push_back(Renderer::MeshCluster{
                    .startIndex     = startIndex,
                    .indexCount     = indexCount,
                    .boundingSphere = Maths::BoundingSphere(center, radius)
                });
            }
        }

        return indices;
    }

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
        const uint32_t totalQuads = (resolutionX - 1) * (resolutionZ - 1);
        const uint32_t totalIndices = totalQuads * 6;

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
            const uint32_t zEnd = std::min(zStart + rowsPerThread, resolutionZ);
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
                        const Vec4 color = generator.EvaluateColor(wx, wy, wz, normal);

                        Vertex& vertex = vertices[rowOffset + ix];
                        vertex.position = Vec3(static_cast<float>(lx), static_cast<float>(ly), static_cast<float>(lz));
                        vertex.normal = normal;
                        vertex.color = color;
                    }
                }
                });
        }
        workers.clear(); // Explicitly join all worker threads before index building

        std::vector<uint32_t> indices = GenerateAlternatingIndices(resolutionX, resolutionZ);

        return TerrainMeshData{
            .vertices = std::move(vertices),
            .indices = std::move(indices)
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
        const uint32_t totalQuads = (resolutionX - 1) * (resolutionZ - 1);
        const uint32_t totalIndices = totalQuads * 6;

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
            const uint32_t zEnd = std::min(zStart + rowsPerThread, resolutionZ);
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
                            color = Vec4(0.48f, 0.46f, 0.44f, 0.0f);
                        }
                        else if (heightRatio > 0.65f)
                        {
                            // High moorland peat, heather, and bent-grass
                            color = Vec4(0.38f, 0.35f, 0.26f, 0.0f);
                        }
                        else
                        {
                            // Lush upland valley pasture
                            color = Vec4(0.28f, 0.42f, 0.22f, 0.0f);
                        }

                        Vertex& vertex = vertices[rowOffset + ix];
                        vertex.position = Vec3(static_cast<float>(lx), static_cast<float>(ly), static_cast<float>(lz));
                        vertex.normal = normal;
                        vertex.color = color;
                    }
                }
                });
        }
        workers.clear(); // Explicitly join all worker threads before index building

        std::vector<uint32_t> indices = GenerateAlternatingIndices(resolutionX, resolutionZ);

        return TerrainMeshData{
            .vertices = std::move(vertices),
            .indices = std::move(indices)
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
                .indices = std::move(indices)
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
                outHeader->version = 1;
                outHeader->originX = header.originX;
                outHeader->originZ = header.originZ;
                outHeader->width = header.width;
                outHeader->depth = header.depth;
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

    namespace
    {
        TerrainMeshData ExtractNorthSectionInternal(
            const TerrainMeshData& fullMesh,
            const MeshFileHeader& fullHeader,
            uint32_t zStart,
            double subDepthRatio,
            MeshFileHeader* outSubHeader
        )
        {
            if (fullMesh.vertices.empty() || fullHeader.vertexCount == 0)
            {
                return {};
            }

            // The regular grid resolution is 1000 x 1000 vertices
            const uint32_t resX = static_cast<uint32_t>(std::round(std::sqrt(static_cast<double>(fullHeader.vertexCount))));
            const uint32_t resZ = resX;
            if (resX * resZ != fullHeader.vertexCount || zStart >= resZ || resZ < 2)
            {
                return {};
            }

            const uint32_t subResZ = resZ - zStart;
            const uint32_t subResX = resX;
            const uint32_t subVertexCount = subResX * subResZ;

            // Calculate the center of the extracted Z range to center the sub-mesh at (0, 0)
            const float zMinLocal = fullMesh.vertices[zStart * resX].position.z;
            const float zMaxLocal = fullMesh.vertices[(resZ - 1) * resX].position.z;
            const float zCenterOffset = (zMinLocal + zMaxLocal) * 0.5f;

            std::vector<Vertex> subVertices;
            subVertices.reserve(subVertexCount);

            float minX = std::numeric_limits<float>::max();
            float maxX = -std::numeric_limits<float>::max();
            float minY = std::numeric_limits<float>::max();
            float maxY = -std::numeric_limits<float>::max();
            float minZ = std::numeric_limits<float>::max();
            float maxZ = -std::numeric_limits<float>::max();

            for (uint32_t iz = zStart; iz < resZ; ++iz)
            {
                const uint32_t rowOffset = iz * resX;
                for (uint32_t ix = 0; ix < resX; ++ix)
                {
                    Vertex v = fullMesh.vertices[rowOffset + ix];
                    v.position.z -= zCenterOffset;

                    minX = std::min(minX, v.position.x);
                    maxX = std::max(maxX, v.position.x);
                    minY = std::min(minY, v.position.y);
                    maxY = std::max(maxY, v.position.y);
                    minZ = std::min(minZ, v.position.z);
                    maxZ = std::max(maxZ, v.position.z);

                    subVertices.push_back(v);
                }
            }

            std::vector<uint32_t> subIndices = TerrainMesh::GenerateAlternatingIndices(subResX, subResZ);
            const uint32_t subIndexCount = static_cast<uint32_t>(subIndices.size());

            if (outSubHeader)
            {
                *outSubHeader = fullHeader;
                outSubHeader->vertexCount = subVertexCount;
                outSubHeader->indexCount = subIndexCount;
                outSubHeader->minX = minX;
                outSubHeader->maxX = maxX;
                outSubHeader->minY = minY;
                outSubHeader->maxY = maxY;
                outSubHeader->minZ = minZ;
                outSubHeader->maxZ = maxZ;
                outSubHeader->width = (maxX > minX) ? static_cast<double>(maxX - minX) : fullHeader.width;
                outSubHeader->depth = (maxZ > minZ) ? static_cast<double>(maxZ - minZ) : (fullHeader.depth * subDepthRatio);
                outSubHeader->minElevation = minY;
                outSubHeader->maxElevation = maxY;
            }

            return TerrainMeshData{
                .vertices = std::move(subVertices),
                .indices = std::move(subIndices)
            };
        }
    }

    TerrainMeshData TerrainMesh::ExtractNorthThird(
        const TerrainMeshData& fullMesh,
        const MeshFileHeader& fullHeader,
        MeshFileHeader* outSubHeader
    )
    {
        const uint32_t resZ = static_cast<uint32_t>(std::round(std::sqrt(static_cast<double>(fullHeader.vertexCount))));
        const uint32_t zStart = (resZ * 2) / 3;
        return ExtractNorthSectionInternal(fullMesh, fullHeader, zStart, 1.0 / 3.0, outSubHeader);
    }

    TerrainMeshData TerrainMesh::ExtractNorthHalf(
        const TerrainMeshData& fullMesh,
        const MeshFileHeader& fullHeader,
        MeshFileHeader* outSubHeader
    )
    {
        // If the mesh is already cropped to the north half (500,000 vertices: 1000 x 500), pass through as-is
        constexpr uint32_t northHalfVertexCount = 500000u;
        constexpr double maxNorthHalfDepth = 7500.0;
        if (fullHeader.vertexCount == northHalfVertexCount || fullHeader.depth <= maxNorthHalfDepth)
        {
            if (outSubHeader)
            {
                *outSubHeader = fullHeader;
            }
            return fullMesh;
        }

        const uint32_t resZ = static_cast<uint32_t>(std::round(std::sqrt(static_cast<double>(fullHeader.vertexCount))));
        const uint32_t zStart = resZ / 2;
        return ExtractNorthSectionInternal(fullMesh, fullHeader, zStart, 0.5, outSubHeader);
    }

    void TerrainMesh::ApplyProceduralPalette(
        std::span<Vertex> vertices,
        const TerrainConfig& config,
        float minElevation,
        float maxElevation,
        uint32_t gridResX,
        uint32_t gridResZ
    )
    {
        if (vertices.empty())
        {
            return;
        }

        // Auto-detect grid resolution if not provided
        if (gridResX == 0 || gridResZ == 0)
        {
            if (vertices.size() == 500000)
            {
                gridResX = 1000;
                gridResZ = 500;
            }
            else if (vertices.size() == 1000000)
            {
                gridResX = 1000;
                gridResZ = 1000;
            }
            else
            {
                const auto squareSide = static_cast<uint32_t>(std::round(std::sqrt(static_cast<double>(vertices.size()))));
                if (static_cast<size_t>(squareSide) * squareSide == vertices.size())
                {
                    gridResX = squareSide;
                    gridResZ = squareSide;
                }
            }
        }

        const float elevSpan = std::max(maxElevation - minElevation, 1.0f);
        const unsigned int hardwareThreads = std::thread::hardware_concurrency();
        const unsigned int numThreads = std::max(1u, hardwareThreads == 0 ? 4u : hardwareThreads);
        const size_t totalVertices = vertices.size();
        const size_t chunkSize = (totalVertices + numThreads - 1) / numThreads;

        std::vector<float> concavities;
        if (gridResX > 4 && gridResZ > 4)
        {
            concavities.resize(totalVertices, 0.0f);
            std::vector<std::jthread> concavityWorkers;
            concavityWorkers.reserve(numThreads);

            for (unsigned int t = 0; t < numThreads; ++t)
            {
                const size_t start = t * chunkSize;
                const size_t end = std::min(start + chunkSize, totalVertices);
                if (start >= end)
                {
                    break;
                }

                concavityWorkers.emplace_back([&, start, end, gridResX, gridResZ]() {
                    namespace TC = TerrainConstants;
                    for (size_t i = start; i < end; ++i)
                    {
                        const uint32_t ix = static_cast<uint32_t>(i % gridResX);
                        const uint32_t iz = static_cast<uint32_t>(i / gridResX);

                        if (ix >= 1 && ix < gridResX - 1 && iz >= 1 && iz < gridResZ - 1)
                        {
                            const float elev = vertices[i].position.y;
                            const float yL = vertices[iz * gridResX + (ix - 1)].position.y;
                            const float yR = vertices[iz * gridResX + (ix + 1)].position.y;
                            const float yD = vertices[(iz - 1) * gridResX + ix].position.y;
                            const float yU = vertices[(iz + 1) * gridResX + ix].position.y;
                            const float laplaceCross = (yL + yR + yD + yU) * 0.25f - elev;

                            const float yDL = vertices[(iz - 1) * gridResX + (ix - 1)].position.y;
                            const float yDR = vertices[(iz - 1) * gridResX + (ix + 1)].position.y;
                            const float yUL = vertices[(iz + 1) * gridResX + (ix - 1)].position.y;
                            const float yUR = vertices[(iz + 1) * gridResX + (ix + 1)].position.y;
                            const float laplaceDiag = (yDL + yDR + yUL + yUR) * 0.25f - elev;

                            const float raw = laplaceCross * TC::Detailing::LaplaceCrossWeight + laplaceDiag * TC::Detailing::LaplaceDiagWeight;
                            concavities[i] = std::max(raw, 0.0f);
                        }
                    }
                    });
            }
            concavityWorkers.clear(); // Explicitly join all concavity workers
        }

        std::vector<std::jthread> workers;
        workers.reserve(numThreads);

        for (unsigned int t = 0; t < numThreads; ++t)
        {
            const size_t start = t * chunkSize;
            const size_t end = std::min(start + chunkSize, totalVertices);
            if (start >= end)
            {
                break;
            }

            workers.emplace_back([&, start, end, gridResX, gridResZ]() {
                namespace TC = TerrainConstants;
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

                    // Organic moorland patches: multi-frequency noise creating naturally feathered boundaries
                    const float moorPatchNoise = noise.Perlin(vertex.position.x * 0.005f, vertex.position.z * 0.005f) * static_cast<float>(TC::River::MeanderNoiseWeight1) +
                        noise.Perlin(vertex.position.x * 0.016f, vertex.position.z * 0.016f) * static_cast<float>(TC::River::MeanderNoiseWeight2);
                    const float rushNoise = noise.Perlin(vertex.position.x * 0.080f, vertex.position.z * 0.080f);

                    // Scale crevice threshold linearly upwards to creviceMaxThreshold between creviceScalingStartAlt and creviceScalingEndAlt
                    float creviceThresh = config.creviceThreshold;
                    if (altNorm < config.creviceScalingStartAlt)
                    {
                        const float altSpan = config.creviceScalingStartAlt - config.creviceScalingEndAlt;
                        const float t = (altSpan > 0.0f)
                            ? std::clamp((altNorm - config.creviceScalingEndAlt) / altSpan, 0.0f, 1.0f)
                            : 1.0f;
                        creviceThresh = std::lerp(config.creviceMaxThreshold, config.creviceThreshold, t);
                    }
                    const float scarGate = std::min(creviceThresh * TC::Detailing::ScarGateRatio, TC::Detailing::ScarGateCeiling);

                    // Topographical concavity evaluation with furrow continuity filtering:
                    // Detects genuine continuous hillside crevices, ravines, and gills while eliminating isolated single-vertex LiDAR noise dots
                    float concavity = 0.0f;
                    if (!concavities.empty())
                    {
                        const uint32_t ix = static_cast<uint32_t>(i % gridResX);
                        const uint32_t iz = static_cast<uint32_t>(i / gridResX);

                        const float rawConcavity = concavities[i];
                        if (rawConcavity > 0.0f && ix >= 1 && ix < gridResX - 1 && iz >= 1 && iz < gridResZ - 1)
                        {
                            const float neighborThreshold = creviceThresh * TC::Detailing::CreviceContinuityRatio;
                            const size_t nIdx[8] = {
                                static_cast<size_t>(iz) * gridResX + (ix - 1),
                                static_cast<size_t>(iz) * gridResX + (ix + 1),
                                static_cast<size_t>(iz - 1) * gridResX + ix,
                                static_cast<size_t>(iz + 1) * gridResX + ix,
                                static_cast<size_t>(iz - 1) * gridResX + (ix - 1),
                                static_cast<size_t>(iz - 1) * gridResX + (ix + 1),
                                static_cast<size_t>(iz + 1) * gridResX + (ix - 1),
                                static_cast<size_t>(iz + 1) * gridResX + (ix + 1)
                            };

                            uint32_t concaveNeighbors = 0;
                            float maxNeighborConcavity = 0.0f;
                            for (size_t n = 0; n < 8; ++n)
                            {
                                const float nc = concavities[nIdx[n]];
                                maxNeighborConcavity = std::max(maxNeighborConcavity, nc);
                                if (nc >= neighborThreshold)
                                {
                                    ++concaveNeighbors;
                                }
                            }

                            // Genuine drainage ravines and gills form continuous multi-vertex channels (at least 2 concave neighbors along the furrow axis).
                            // Isolated single-vertex pits (0 concave neighbors) are completely suppressed.
                            if (concaveNeighbors >= 2)
                            {
                                concavity = rawConcavity;
                            }
                            else if (concaveNeighbors == 1)
                            {
                                const float support = std::clamp((maxNeighborConcavity - neighborThreshold) / (creviceThresh * TC::Detailing::CreviceContinuityRamp), 0.0f, 1.0f);
                                concavity = rawConcavity * (0.5f * support);
                            }
                            else
                            {
                                concavity = 0.0f;
                            }
                        }
                    }

                    // Altitudinal vegetation belts matching British upland ecology
                    Maths::Vec4 baseVegColor;
                    if (altNorm < config.valleyPastureMaxAlt)
                    {
                        const float rawFactor = altNorm / config.valleyPastureMaxAlt;
                        const float smoothFactor = rawFactor * rawFactor * (3.0f - 2.0f * rawFactor);
                        baseVegColor = config.valleyFloorColor.Lerp(config.lowSlopeColor, smoothFactor);
                    }
                    else if (altNorm < config.lowerSlopeMaxAlt)
                    {
                        const float rawFactor = (altNorm - config.valleyPastureMaxAlt) / (config.lowerSlopeMaxAlt - config.valleyPastureMaxAlt);
                        const float smoothFactor = rawFactor * rawFactor * (3.0f - 2.0f * rawFactor);
                        baseVegColor = config.lowSlopeColor.Lerp(config.midSlopeColor, smoothFactor);
                    }
                    else
                    {
                        // Upper slopes and high fell plateau (Baugh Fell summit):
                        // Sunlit golden-straw mat-grass and fescue turf
                        const float rawFactor = std::clamp((altNorm - config.lowerSlopeMaxAlt) / (1.0f - config.lowerSlopeMaxAlt), 0.0f, 1.0f);
                        const float smoothFactor = rawFactor * rawFactor * (3.0f - 2.0f * rawFactor);
                        baseVegColor = config.midSlopeColor.Lerp(config.highPlateauColor, smoothFactor);

                        // Heather moorland accents on upper slopes: subtly and smoothly blended into fescues
                        if (altNorm > config.heatherAltitudeThreshold && moorPatchNoise > TC::Detailing::HeatherNoiseThreshold)
                        {
                            const float rawT = std::clamp((moorPatchNoise - TC::Detailing::HeatherNoiseThreshold) / TC::Detailing::HeatherNoiseRamp, 0.0f, 1.0f);
                            const float smoothT = rawT * rawT * (3.0f - 2.0f * rawT);
                            const float heatherWeight = smoothT * TC::Detailing::HeatherMaxWeight;
                            baseVegColor = baseVegColor.Lerp(config.heatherColor, heatherWeight);
                        }

                        // Localized peat hollows: gentle, softly blended depressions
                        // (restricted strictly to hollows, never forming harsh dark blotches)
                        if (altNorm > config.peatAltitudeThreshold && moorPatchNoise < TC::Detailing::PeatNoiseThreshold && slope < config.peatMoorMaxSlope)
                        {
                            const float rawT = std::clamp((-moorPatchNoise + TC::Detailing::PeatNoiseThreshold) / TC::Detailing::PeatNoiseRamp, 0.0f, 1.0f);
                            const float smoothT = rawT * rawT * (3.0f - 2.0f * rawT);
                            const float peatWeight = smoothT * TC::Detailing::PeatMaxWeight;
                            baseVegColor = baseVegColor.Lerp(config.peatMoorColor, peatWeight);
                        }
                    }

                    // Stepped cyclothem limestone scars and sheer crags
                    Maths::Vec4 finalColor;
                    if (slope > config.sheerCragSlope)
                    {
                        // Sheer rock faces and steep crags (> 45 degrees)
                        const float factor = std::clamp((slope - config.sheerCragSlope) / TC::Detailing::SheerCragSlopeRamp, 0.0f, 1.0f);
                        const float smoothFactor = factor * factor * (3.0f - 2.0f * factor);
                        finalColor = config.rockColor.Lerp(config.steepCragColor, smoothFactor);
                    }
                    else if (slope > config.limestoneScarSlope)
                    {
                        // Stepped limestone scar risers and scree benches (~35 to 45 degrees)
                        // Restricted to structural benches where concavity is low (not inside drainage furrows)
                        const float rawT = std::clamp((slope - config.limestoneScarSlope) / TC::Detailing::LimestoneScarSlopeRamp, 0.0f, 1.0f);
                        const float factor = rawT * rawT * (3.0f - 2.0f * rawT);

                        // Continuous concavity gating feathers scar presence smoothly rather than imposing a binary cutoff
                        const float concavityGateFactor = (concavity < scarGate)
                            ? 1.0f
                            : std::clamp(1.0f - (concavity - scarGate) / TC::Detailing::ScarGateRamp, 0.0f, 1.0f);
                        const float smoothGate = concavityGateFactor * concavityGateFactor * (3.0f - 2.0f * concavityGateFactor);
                        const float scarBlend = factor * smoothGate;

                        const Maths::Vec4 scarTone = config.limestoneScarColor.Lerp(config.rockColor, TC::Detailing::ScarToneRockRatio);
                        finalColor = baseVegColor.Lerp(scarTone, scarBlend);
                    }
                    else
                    {
                        // Fully vegetated fell flanks and pastures
                        finalColor = baseVegColor;
                    }

                    float creviceFactor = 0.0f;

                    // Crevice bed detailing: subtle weathered rock accents along incised hillside furrows and gills
                    if (concavity > creviceThresh)
                    {
                        const float bedRaw = std::clamp((concavity - creviceThresh) / config.creviceConcavityRamp, 0.0f, 1.0f);
                        const float bedFactor = bedRaw * bedRaw * (3.0f - 2.0f * bedRaw);

                        // Variegated rocky bed: spatial noise creates a natural mixture of lighter cobbles and darker wet stone
                        const float stoneNoise = noise.Perlin(vertex.position.x * 0.055f, vertex.position.z * 0.055f) * static_cast<float>(TC::River::MeanderNoiseWeight1) +
                            noise.Perlin(vertex.position.x * 0.150f, vertex.position.z * 0.150f) * static_cast<float>(TC::River::MeanderNoiseWeight2);

                        // Lighter shade: pale limestone scar cobbles and riverbed gravel
                        const Maths::Vec4 lightShade = config.riverbedColor.Lerp(config.limestoneScarColor, TC::Detailing::LightShadeLimestoneRatio);

                        // Medium shade: weathered gritstone rock and gravel
                        const Maths::Vec4 midShade = config.riverbedColor.Lerp(config.rockColor, TC::Detailing::MidShadeRockRatio);

                        // Darker shade: damp stone and shadowed crag
                        const Maths::Vec4 darkShade = config.rockColor.Lerp(config.steepCragColor, TC::Detailing::DarkShadeSteepCragRatio);

                        // Select shade based on local stone noise and channel steepness
                        const float steepBias = std::clamp((slope - TC::Detailing::SteepBiasSlopeThreshold) / TC::Detailing::SteepBiasSlopeRamp, 0.0f, 1.0f);
                        Maths::Vec4 creviceBedColor;
                        if (stoneNoise > 0.0f)
                        {
                            const float lightFactor = std::clamp(stoneNoise / TC::Detailing::StoneNoiseRamp, 0.0f, 1.0f);
                            creviceBedColor = midShade.Lerp(lightShade, lightFactor);
                        }
                        else
                        {
                            const float darkFactor = std::clamp(-stoneNoise / TC::Detailing::StoneNoiseRamp, 0.0f, 1.0f);
                            creviceBedColor = midShade.Lerp(darkShade, darkFactor);
                        }

                        // Steeper sections expose slightly darker stone
                        creviceBedColor = creviceBedColor.Lerp(darkShade, steepBias * TC::Detailing::SteepBiasMaxWeight);

                        // Soft, well-balanced blending into the hillside vegetation
                        constexpr float creviceMaxDampening = 0.55f;
                        const float blendStrength = bedFactor * (config.creviceMaxBlendStrength * creviceMaxDampening);
                        finalColor = finalColor.Lerp(creviceBedColor, blendStrength);
                        creviceFactor = bedFactor;
                    }

                    // Apply subtle rush clump accents on lower and mid slopes
                    if (altNorm < config.rushAltitudeMax && rushNoise > TC::Detailing::RushNoiseThreshold && slope < config.rushMaxSlope)
                    {
                        const float rawT = std::clamp((rushNoise - TC::Detailing::RushNoiseThreshold) / TC::Detailing::RushNoiseRamp, 0.0f, 1.0f);
                        const float smoothT = rawT * rawT * (3.0f - 2.0f * rawT);
                        finalColor = finalColor.Lerp(config.lowSlopeColor, smoothT * TC::Detailing::RushMaxWeight);
                    }

                    // Apply subtle organic luminance variation
                    finalColor.x = std::clamp(finalColor.x + mottling, 0.0f, 1.0f);
                    finalColor.y = std::clamp(finalColor.y + mottling, 0.0f, 1.0f);
                    finalColor.z = std::clamp(finalColor.z + mottling, 0.0f, 1.0f);
                    finalColor.w = creviceFactor;

                    vertex.color = finalColor;
                }
                });
        }
        workers.clear();

        // Multi-pass spatial binomial smoothing filter: eliminates 15m quad diagonal saw-tooth
        // artefacts by smoothly diffusing transition boundaries across neighbouring grid vertices
        if (gridResX > 4 && gridResZ > 4)
        {
            std::vector<Maths::Vec4> srcColors(totalVertices);
            for (size_t i = 0; i < totalVertices; ++i)
            {
                srcColors[i] = vertices[i].color;
            }

            std::vector<Maths::Vec4> dstColors = srcColors;

            for (int pass = 0; pass < 3; ++pass)
            {
                for (uint32_t iz = 1; iz < gridResZ - 1; ++iz)
                {
                    const uint32_t rowC = iz * gridResX;
                    const uint32_t rowD = (iz - 1) * gridResX;
                    const uint32_t rowU = (iz + 1) * gridResX;

                    for (uint32_t ix = 1; ix < gridResX - 1; ++ix)
                    {
                        const Maths::Vec4 ortho = srcColors[rowC + ix - 1] + srcColors[rowC + ix + 1] + srcColors[rowD + ix] + srcColors[rowU + ix];
                        const Maths::Vec4 diag = srcColors[rowD + ix - 1] + srcColors[rowD + ix + 1] + srcColors[rowU + ix - 1] + srcColors[rowU + ix + 1];
                        dstColors[rowC + ix] = srcColors[rowC + ix] * 0.36f + ortho * 0.11f + diag * 0.05f;
                    }
                }
                srcColors = dstColors;
            }

            for (size_t i = 0; i < totalVertices; ++i)
            {
                vertices[i].color = dstColors[i];
            }
        }
    }

    LidarOcclusionMaps TerrainMesh::ComputeLidarHorizonOcclusion(
        std::span<const float> elevations,
        uint32_t resolutionX,
        uint32_t resolutionZ,
        double width,
        double depth
    )
    {
        if (elevations.empty() || resolutionX < 2 || resolutionZ < 2)
        {
            return {};
        }

        const size_t totalPixels = static_cast<size_t>(resolutionX) * resolutionZ;
        constexpr size_t channelsPerPixel = 4u;
        std::vector<float> aoFloat(totalPixels * channelsPerPixel, 1.0f);
        std::vector<float> horizonFloat(totalPixels * 8u, 0.0f);

        const float cellSpacingX = static_cast<float>(width / std::max(1u, resolutionX - 1));
        const float cellSpacingZ = static_cast<float>(depth / std::max(1u, resolutionZ - 1));
        const float invCellSpacingX = 1.0f / std::max(cellSpacingX, 0.001f);
        const float invCellSpacingZ = 1.0f / std::max(cellSpacingZ, 0.001f);
        const float maxGridX = static_cast<float>(resolutionX - 1);
        const float maxGridZ = static_cast<float>(resolutionZ - 1);
        constexpr float twoPi = 6.28318530718f;

        // Precompute sixteen directional unit vectors around the horizon azimuth for local ambient occlusion
        constexpr uint32_t numAoDirections = 16;
        std::array<std::pair<float, float>, numAoDirections> aoRayDirections{};
        for (uint32_t d = 0; d < numAoDirections; ++d)
        {
            const float angle = (static_cast<float>(d) / static_cast<float>(numAoDirections)) * twoPi;
            aoRayDirections[d] = { std::cos(angle), std::sin(angle) };
        }

        // Precompute logarithmic distance schedule for ambient occlusion raymarching (12m to 380m)
        constexpr uint32_t aoRaySteps = 16;
        constexpr float minAoDistMeters = 12.0f;
        constexpr float maxAoDistMeters = 380.0f;
        constexpr float aoDistExponent = 1.85f;
        std::array<float, aoRaySteps> aoStepDistances{};
        for (uint32_t s = 0; s < aoRaySteps; ++s)
        {
            const float t = static_cast<float>(s) / static_cast<float>(aoRaySteps - 1);
            aoStepDistances[s] = minAoDistMeters + (maxAoDistMeters - minAoDistMeters) * std::pow(t, aoDistExponent);
        }

        // Precompute eight cardinal and intercardinal directional unit vectors for multi-scale horizon shadows
        constexpr uint32_t numHorizonDirections = 8;
        std::array<std::pair<float, float>, numHorizonDirections> horizonRayDirections{};
        for (uint32_t d = 0; d < numHorizonDirections; ++d)
        {
            const float angle = (static_cast<float>(d) / static_cast<float>(numHorizonDirections)) * twoPi;
            horizonRayDirections[d] = { std::cos(angle), std::sin(angle) };
        }

        // Precompute logarithmic distance schedule for horizon shadow raymarching (15m local gullies out to 3,600m fells)
        constexpr uint32_t horizonRaySteps = 32;
        constexpr float minHorizonDistMeters = 15.0f;
        constexpr float maxHorizonDistMeters = 3600.0f;
        constexpr float horizonDistExponent = 2.15f;
        std::array<float, horizonRaySteps> horizonStepDistances{};
        for (uint32_t s = 0; s < horizonRaySteps; ++s)
        {
            const float t = static_cast<float>(s) / static_cast<float>(horizonRaySteps - 1);
            horizonStepDistances[s] = minHorizonDistMeters + (maxHorizonDistMeters - minHorizonDistMeters) * std::pow(t, horizonDistExponent);
        }

        const unsigned int hardwareThreads = std::thread::hardware_concurrency();
        const unsigned int numThreads = std::max(1u, hardwareThreads == 0 ? 4u : hardwareThreads);
        const uint32_t rowsPerThread = (resolutionZ + numThreads - 1) / numThreads;

        std::vector<std::jthread> workers;
        workers.reserve(numThreads);

        for (unsigned int t = 0; t < numThreads; ++t)
        {
            const uint32_t zStart = t * rowsPerThread;
            const uint32_t zEnd = std::min(zStart + rowsPerThread, resolutionZ);
            if (zStart >= zEnd)
            {
                break;
            }

            workers.emplace_back([&, zStart, zEnd]() {
                constexpr float localClearanceBiasMeters = 0.35f;
                constexpr float minAmbientVisibility = 0.05f;

                // Continuous bilinear elevation sampler eliminating nearest-grid staircase artefacts along diagonal rays
                auto sampleBilinearElevation = [&](float gx, float gz) noexcept -> float {
                    const float clampedX = std::clamp(gx, 0.0f, maxGridX);
                    const float clampedZ = std::clamp(gz, 0.0f, maxGridZ);
                    const uint32_t x0 = static_cast<uint32_t>(clampedX);
                    const uint32_t z0 = static_cast<uint32_t>(clampedZ);
                    const uint32_t x1 = std::min(x0 + 1u, resolutionX - 1u);
                    const uint32_t z1 = std::min(z0 + 1u, resolutionZ - 1u);
                    const float fx = clampedX - static_cast<float>(x0);
                    const float fz = clampedZ - static_cast<float>(z0);

                    const size_t row0 = static_cast<size_t>(z0) * resolutionX;
                    const size_t row1 = static_cast<size_t>(z1) * resolutionX;
                    const float h00 = elevations[row0 + x0];
                    const float h10 = elevations[row0 + x1];
                    const float h01 = elevations[row1 + x0];
                    const float h11 = elevations[row1 + x1];

                    const float h0 = h00 + fx * (h10 - h00);
                    const float h1 = h01 + fx * (h11 - h01);
                    return h0 + fz * (h1 - h0);
                };

                for (uint32_t iz = zStart; iz < zEnd; ++iz)
                {
                    const float originWorldZ = static_cast<float>(iz) * cellSpacingZ;

                    for (uint32_t ix = 0; ix < resolutionX; ++ix)
                    {
                        const size_t centerIndex = static_cast<size_t>(iz) * resolutionX + ix;
                        const float centerElev = elevations[centerIndex];
                        const float originWorldX = static_cast<float>(ix) * cellSpacingX;

                        // Evaluate multi-directional horizon sky line-of-sight exposure for local ambient occlusion
                        float totalSkyVisibility = 0.0f;
                        for (uint32_t d = 0; d < numAoDirections; ++d)
                        {
                            const auto& [dirX, dirZ] = aoRayDirections[d];
                            float maxTanSlope = 0.0f;

                            for (uint32_t s = 0; s < aoRaySteps; ++s)
                            {
                                const float sampleDist = aoStepDistances[s];
                                const float gx = (originWorldX + dirX * sampleDist) * invCellSpacingX;
                                const float gz = (originWorldZ + dirZ * sampleDist) * invCellSpacingZ;

                                if (gx < 0.0f || gx > maxGridX || gz < 0.0f || gz > maxGridZ)
                                {
                                    break;
                                }

                                const float sampleElev = sampleBilinearElevation(gx, gz);
                                const float deltaElev = sampleElev - centerElev - localClearanceBiasMeters;

                                if (deltaElev > 0.0f)
                                {
                                    const float tanSlope = deltaElev / sampleDist;
                                    maxTanSlope = std::max(maxTanSlope, tanSlope);
                                }
                            }

                            // Solid angle sky visibility factor along this azimuthal slice
                            const float sinHorizonAngle = maxTanSlope / std::sqrt(1.0f + maxTanSlope * maxTanSlope);
                            const float sliceVisibility = std::clamp(1.0f - sinHorizonAngle, 0.0f, 1.0f);
                            totalSkyVisibility += sliceVisibility;
                        }

                        const float horizonAO = std::clamp(totalSkyVisibility / static_cast<float>(numAoDirections), minAmbientVisibility, 1.0f);

                        // Evaluate multi-scale horizon elevation angles across eight azimuthal directions
                        const size_t horizonBaseOffset = centerIndex * numHorizonDirections;
                        for (uint32_t d = 0; d < numHorizonDirections; ++d)
                        {
                            const auto& [dirX, dirZ] = horizonRayDirections[d];
                            float maxTanSlope = 0.0f;

                            for (uint32_t s = 0; s < horizonRaySteps; ++s)
                            {
                                const float sampleDist = horizonStepDistances[s];
                                const float gx = (originWorldX + dirX * sampleDist) * invCellSpacingX;
                                const float gz = (originWorldZ + dirZ * sampleDist) * invCellSpacingZ;

                                if (gx < 0.0f || gx > maxGridX || gz < 0.0f || gz > maxGridZ)
                                {
                                    break;
                                }

                                const float sampleElev = sampleBilinearElevation(gx, gz);
                                const float deltaElev = sampleElev - centerElev - localClearanceBiasMeters;

                                if (deltaElev > 0.0f)
                                {
                                    const float tanSlope = deltaElev / sampleDist;
                                    maxTanSlope = std::max(maxTanSlope, tanSlope);
                                }
                            }

                            const float sinElevation = (maxTanSlope > 0.0f)
                                ? (maxTanSlope / std::sqrt(1.0f + maxTanSlope * maxTanSlope))
                                : 0.0f;
                            horizonFloat[horizonBaseOffset + d] = std::clamp(sinElevation, 0.0f, 1.0f);
                        }

                        // Multi-scale topographical curvature evaluating local and intermediate crevice incision
                        auto sampleElevAt = [&](int x, int z) noexcept -> float {
                            const int cx = std::clamp(x, 0, static_cast<int>(resolutionX - 1));
                            const int cz = std::clamp(z, 0, static_cast<int>(resolutionZ - 1));
                            return elevations[static_cast<size_t>(cz) * resolutionX + static_cast<size_t>(cx)];
                            };

                        const int cx = static_cast<int>(ix);
                        const int cz = static_cast<int>(iz);

                        // Fine-scale Laplacian across 1-cell radius (~15m)
                        const float fineSurround = (
                            sampleElevAt(cx - 1, cz) + sampleElevAt(cx + 1, cz) +
                            sampleElevAt(cx, cz - 1) + sampleElevAt(cx, cz + 1) +
                            sampleElevAt(cx - 1, cz - 1) + sampleElevAt(cx + 1, cz - 1) +
                            sampleElevAt(cx - 1, cz + 1) + sampleElevAt(cx + 1, cz + 1)
                            ) * 0.125f;
                        const float fineConcavity = std::max(fineSurround - centerElev, 0.0f);

                        // Intermediate-scale Laplacian across 3-cell radius (~45m)
                        const float midSurround = (
                            sampleElevAt(cx - 3, cz) + sampleElevAt(cx + 3, cz) +
                            sampleElevAt(cx, cz - 3) + sampleElevAt(cx, cz + 3) +
                            sampleElevAt(cx - 2, cz - 2) + sampleElevAt(cx + 2, cz - 2) +
                            sampleElevAt(cx - 2, cz + 2) + sampleElevAt(cx + 2, cz + 2)
                            ) * 0.125f;
                        const float midConcavity = std::max(midSurround - centerElev, 0.0f);

                        // Broad-scale Laplacian across 6-cell radius (~90m)
                        const float broadSurround = (
                            sampleElevAt(cx - 6, cz) + sampleElevAt(cx + 6, cz) +
                            sampleElevAt(cx, cz - 6) + sampleElevAt(cx, cz + 6)
                            ) * 0.25f;
                        const float broadConcavity = std::max(broadSurround - centerElev, 0.0f);

                        // Continuity and support gating for fine concavity:
                        // Prevents isolated single-cell micro-pits in the elevation data from appearing as baked crevice dots
                        float filteredFineConcavity = fineConcavity;
                        if (fineConcavity > 0.35f)
                        {
                            auto evalFineAt = [&](int x, int z) noexcept -> float {
                                const float c = sampleElevAt(x, z);
                                const float s = (
                                    sampleElevAt(x - 1, z) + sampleElevAt(x + 1, z) +
                                    sampleElevAt(x, z - 1) + sampleElevAt(x, z + 1) +
                                    sampleElevAt(x - 1, z - 1) + sampleElevAt(x + 1, z - 1) +
                                    sampleElevAt(x - 1, z + 1) + sampleElevAt(x + 1, z + 1)
                                    ) * 0.125f;
                                return std::max(s - c, 0.0f);
                                };

                            const float n0 = evalFineAt(cx - 1, cz);
                            const float n1 = evalFineAt(cx + 1, cz);
                            const float n2 = evalFineAt(cx, cz - 1);
                            const float n3 = evalFineAt(cx, cz + 1);
                            const float maxNeighbor = std::max({ n0, n1, n2, n3 });

                            if (maxNeighbor < 0.15f)
                            {
                                filteredFineConcavity = 0.0f;
                            }
                            else
                            {
                                const float continuitySupport = std::clamp((maxNeighbor - 0.15f) / 0.25f, 0.0f, 1.0f);
                                filteredFineConcavity = fineConcavity * continuitySupport;
                            }
                        }
                        else
                        {
                            filteredFineConcavity = 0.0f;
                        }

                        // For intermediate and broad concavity, require furrow incision depth expanding across scale
                        const float furrowIncisionDepth = std::max(midSurround - fineSurround, 0.0f);
                        const float intermediateSupport = std::clamp(furrowIncisionDepth / 0.30f, 0.0f, 1.0f);
                        const float filteredMidConcavity = midConcavity * intermediateSupport;
                        const float filteredBroadConcavity = broadConcavity * intermediateSupport;

                        // Normalised crevice depth factor
                        const float combinedConcavity = filteredFineConcavity * 0.45f + filteredMidConcavity * 0.35f + filteredBroadConcavity * 0.20f;
                        constexpr float creviceDepthScale = 4.5f;
                        const float rawCrevice = std::clamp(combinedConcavity / creviceDepthScale, 0.0f, 1.0f);
                        const float creviceFactor = rawCrevice * rawCrevice * (3.0f - 2.0f * rawCrevice);

                        // Crest exposure highlighting exposed ridges and knolls
                        const float crestExposure = std::clamp((centerElev - fineSurround) / creviceDepthScale, 0.0f, 1.0f);

                        const size_t pixelOffset = centerIndex * channelsPerPixel;
                        aoFloat[pixelOffset + 0] = horizonAO;
                        aoFloat[pixelOffset + 1] = creviceFactor;
                        aoFloat[pixelOffset + 2] = crestExposure;
                        aoFloat[pixelOffset + 3] = 1.0f;
                    }
                }
                });
        }

        for (auto& worker : workers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }

        // Multi-pass 3x3 spatial binomial smoothing in floating-point to eliminate 8-bit quantization banding and grid stepping
        if (resolutionX > 4 && resolutionZ > 4)
        {
            constexpr int smoothingPasses = 2;
            constexpr float centerWeight = 0.36f;
            constexpr float orthoWeight  = 0.11f;
            constexpr float diagWeight   = 0.05f;

            std::vector<float> dstAo = aoFloat;
            std::vector<float> dstHorizon = horizonFloat;

            for (int pass = 0; pass < smoothingPasses; ++pass)
            {
                for (uint32_t iz = 1; iz < resolutionZ - 1; ++iz)
                {
                    const size_t rowC = static_cast<size_t>(iz) * resolutionX;
                    const size_t rowD = static_cast<size_t>(iz - 1) * resolutionX;
                    const size_t rowU = static_cast<size_t>(iz + 1) * resolutionX;

                    for (uint32_t ix = 1; ix < resolutionX - 1; ++ix)
                    {
                        const size_t idxC  = rowC + ix;
                        const size_t idxL  = rowC + ix - 1;
                        const size_t idxR  = rowC + ix + 1;
                        const size_t idxD  = rowD + ix;
                        const size_t idxU  = rowU + ix;
                        const size_t idxDL = rowD + ix - 1;
                        const size_t idxDR = rowD + ix + 1;
                        const size_t idxUL = rowU + ix - 1;
                        const size_t idxUR = rowU + ix + 1;

                        // Smooth AO, crevice depth, and crest exposure channels
                        const size_t c4 = idxC * channelsPerPixel;
                        for (size_t ch = 0; ch < 3; ++ch)
                        {
                            const float ortho = aoFloat[idxL * channelsPerPixel + ch] + aoFloat[idxR * channelsPerPixel + ch] +
                                                aoFloat[idxD * channelsPerPixel + ch] + aoFloat[idxU * channelsPerPixel + ch];
                            const float diag  = aoFloat[idxDL * channelsPerPixel + ch] + aoFloat[idxDR * channelsPerPixel + ch] +
                                                aoFloat[idxUL * channelsPerPixel + ch] + aoFloat[idxUR * channelsPerPixel + ch];
                            dstAo[c4 + ch] = std::clamp(aoFloat[c4 + ch] * centerWeight + ortho * orthoWeight + diag * diagWeight, 0.0f, 1.0f);
                        }

                        // Smooth all eight azimuthal horizon elevation channels
                        const size_t c8 = idxC * numHorizonDirections;
                        for (size_t d = 0; d < numHorizonDirections; ++d)
                        {
                            const float ortho = horizonFloat[idxL * numHorizonDirections + d] + horizonFloat[idxR * numHorizonDirections + d] +
                                                horizonFloat[idxD * numHorizonDirections + d] + horizonFloat[idxU * numHorizonDirections + d];
                            const float diag  = horizonFloat[idxDL * numHorizonDirections + d] + horizonFloat[idxDR * numHorizonDirections + d] +
                                                horizonFloat[idxUL * numHorizonDirections + d] + horizonFloat[idxUR * numHorizonDirections + d];
                            dstHorizon[c8 + d] = std::clamp(horizonFloat[c8 + d] * centerWeight + ortho * orthoWeight + diag * diagWeight, 0.0f, 1.0f);
                        }
                    }
                }
                aoFloat = dstAo;
                horizonFloat = dstHorizon;
            }
        }

        // Pack filtered floating-point occlusion and horizon maps into 8-bit UNORM texture buffers
        std::vector<uint8_t> aoData(totalPixels * channelsPerPixel, 255u);
        std::vector<uint8_t> horizon0Data(totalPixels * channelsPerPixel, 0u);
        std::vector<uint8_t> horizon1Data(totalPixels * channelsPerPixel, 0u);
        constexpr float byteScale = 255.0f;
        constexpr float roundingBias = 0.5f;

        for (size_t i = 0; i < totalPixels; ++i)
        {
            const size_t c4 = i * channelsPerPixel;
            const size_t c8 = i * numHorizonDirections;

            aoData[c4 + 0] = static_cast<uint8_t>( std::clamp(aoFloat[c4 + 0] * byteScale + roundingBias, 0.0f, byteScale) );
            aoData[c4 + 1] = static_cast<uint8_t>( std::clamp(aoFloat[c4 + 1] * byteScale + roundingBias, 0.0f, byteScale) );
            aoData[c4 + 2] = static_cast<uint8_t>( std::clamp(aoFloat[c4 + 2] * byteScale + roundingBias, 0.0f, byteScale) );
            aoData[c4 + 3] = 255u;

            for (size_t d = 0; d < channelsPerPixel; ++d)
            {
                horizon0Data[c4 + d] = static_cast<uint8_t>( std::clamp(horizonFloat[c8 + d] * byteScale + roundingBias, 0.0f, byteScale) );
                horizon1Data[c4 + d] = static_cast<uint8_t>( std::clamp(horizonFloat[c8 + d + channelsPerPixel] * byteScale + roundingBias, 0.0f, byteScale) );
            }
        }

        LidarOcclusionMaps maps;
        maps.ambientOcclusionMap = std::move(aoData);
        maps.horizonAnglesMap0 = std::move(horizon0Data);
        maps.horizonAnglesMap1 = std::move(horizon1Data);
        return maps;
    }
}
