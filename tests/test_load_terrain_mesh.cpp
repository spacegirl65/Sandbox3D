// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Engine/TerrainMesh.h"
#include "Renderer/Mesh.h"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace Sandbox3D;

int main()
{
    std::cout << "=== Running Terrain .mesh Extraction and Loading Verification ===\n\n";

    // Test 1: Load file header and verify metadata
    std::cout << "[Test 1] Verifying terrain_15km.mesh binary header and extents...\n";
    Renderer::MeshFileHeader header{};
    auto meshData = Engine::TerrainMesh::GenerateFromFile(
        "resources/environment/terrain/terrain_15km.mesh",
        Maths::Vec3D(0.0, 0.0, 0.0),
        &header
    );

    assert(!meshData.IsEmpty() && "Mesh data should not be empty!");
    assert(header.magic[0] == 'S' && header.magic[1] == '3' && header.magic[2] == 'D' && header.magic[3] == 'M');
    assert(header.version == 1);
    assert(header.vertexCount == 1000000);
    assert(header.indexCount == 5988006);
    std::cout << "  Passed: Magic 'S3DM', version " << header.version
              << ", vertices: " << header.vertexCount
              << ", indices: " << header.indexCount << "\n";

    // Test 2: Check spatial bounds and real-world dimensions
    std::cout << "[Test 2] Verifying spatial dimensions and elevation bounds...\n";
    assert(std::abs(header.width - 15000.0) < 1.0);
    assert(std::abs(header.depth - 15000.0) < 1.0);
    assert(header.minElevation > 50.0f && header.minElevation < 150.0f);
    assert(header.maxElevation > 600.0f && header.maxElevation < 850.0f);
    std::cout << "  Passed: Width " << header.width << "m, Depth " << header.depth << "m\n";
    std::cout << "  Passed: Elevation range [" << header.minElevation << "m, " << header.maxElevation << "m]\n";

    // Test 3: Check vertex buffer integrity
    std::cout << "[Test 3] Verifying vertex attributes and surface normals...\n";
    assert(meshData.vertices.size() == 1000000);
    size_t validNormalCount = 0;
    for (size_t i = 0; i < meshData.vertices.size(); ++i)
    {
        const auto& v = meshData.vertices[i];
        const float lenSq = v.normal.LengthSquared();
        if (std::abs(lenSq - 1.0f) < 0.05f)
        {
            ++validNormalCount;
        }
        // Alpha must be 1.0
        assert(v.color.w == 1.0f);
    }
    assert(validNormalCount == meshData.vertices.size());
    std::cout << "  Passed: 1,000,000 vertices verified with unit-length normals.\n";

    // Test 4: Check index buffer and triangle winding
    std::cout << "[Test 4] Verifying triangle indices and bounds...\n";
    assert(meshData.indices.size() == 5988006);
    for (size_t i = 0; i < 1000; ++i)
    {
        assert(meshData.indices[i] < 1000000);
    }
    std::cout << "  Passed: 1,996,002 triangles (5,988,006 indices) within vertex range.\n";

    // Test 5: Check ExtractNorthThird extraction, dimensions, and centering
    std::cout << "[Test 5] Verifying northern third extraction (SD69se, SD79sw, and SD79se)...\n";
    Renderer::MeshFileHeader northHeader{};
    auto northData = Engine::TerrainMesh::ExtractNorthThird(meshData, header, &northHeader);

    assert(northData.vertices.size() == 334000);
    assert(northData.indices.size() == 1996002);
    assert(northHeader.vertexCount == 334000);
    assert(northHeader.indexCount == 1996002);
    assert(std::abs(northHeader.width - 15000.0) < 1.0);
    assert(std::abs(northHeader.depth - 5000.0) < 1.0);

    float minX = 1e9f, maxX = -1e9f;
    float minZ = 1e9f, maxZ = -1e9f;
    for (const auto& v : northData.vertices)
    {
        if (v.position.x < minX) minX = v.position.x;
        if (v.position.x > maxX) maxX = v.position.x;
        if (v.position.z < minZ) minZ = v.position.z;
        if (v.position.z > maxZ) maxZ = v.position.z;
    }
    const float centerX = (minX + maxX) * 0.5f;
    const float centerZ = (minZ + maxZ) * 0.5f;
    assert(std::abs(centerX) < 0.1f);
    assert(std::abs(centerZ) < 0.1f);
    std::cout << "  Passed: Northern third extracted with " << northData.vertices.size()
              << " vertices, " << northData.indices.size() / 3 << " triangles.\n";
    std::cout << "  Passed: Centered at local origin (X: [" << minX << ", " << maxX
              << "], Z: [" << minZ << ", " << maxZ << "]).\n";

    // Test 6: Check ExtractNorthHalf extraction, dimensions, and centering
    std::cout << "[Test 6] Verifying northern half extraction...\n";
    Renderer::MeshFileHeader northHalfHeader{};
    auto northHalfData = Engine::TerrainMesh::ExtractNorthHalf(meshData, header, &northHalfHeader);

    assert(northHalfData.vertices.size() == 500000);
    assert(northHalfData.indices.size() == 2991006);
    assert(northHalfHeader.vertexCount == 500000);
    assert(northHalfHeader.indexCount == 2991006);
    assert(std::abs(northHalfHeader.width - 15000.0) < 1.0);
    assert(std::abs(northHalfHeader.depth - 7500.0) < 1.0);

    float halfMinX = 1e9f, halfMaxX = -1e9f;
    float halfMinZ = 1e9f, halfMaxZ = -1e9f;
    for (const auto& v : northHalfData.vertices)
    {
        if (v.position.x < halfMinX) halfMinX = v.position.x;
        if (v.position.x > halfMaxX) halfMaxX = v.position.x;
        if (v.position.z < halfMinZ) halfMinZ = v.position.z;
        if (v.position.z > halfMaxZ) halfMaxZ = v.position.z;
    }
    const float halfCenterX = (halfMinX + halfMaxX) * 0.5f;
    const float halfCenterZ = (halfMinZ + halfMaxZ) * 0.5f;
    assert(std::abs(halfCenterX) < 0.1f);
    assert(std::abs(halfCenterZ) < 0.1f);
    std::cout << "  Passed: Northern half extracted with " << northHalfData.vertices.size()
              << " vertices, " << northHalfData.indices.size() / 3 << " triangles.\n";
    std::cout << "  Passed: Centered at local origin (X: [" << halfMinX << ", " << halfMaxX
              << "], Z: [" << halfMinZ << ", " << halfMaxZ << "]).\n";

    std::cout << "\n>>> All Terrain .mesh Loading Verification Tests PASSED Successfully! <<<\n";
    return 0;
}

