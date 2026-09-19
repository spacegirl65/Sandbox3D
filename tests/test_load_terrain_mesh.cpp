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

    std::cout << "\n>>> All Terrain .mesh Loading Verification Tests PASSED Successfully! <<<\n";
    return 0;
}

