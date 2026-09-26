// Copyright © 2026 spacegirl65. All Rights Reserved.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>

#pragma pack(push, 1)
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

struct Vec3
{
    float x{ 0.0f };
    float y{ 0.0f };
    float z{ 0.0f };
};

struct Vec4
{
    float x{ 0.0f };
    float y{ 0.0f };
    float z{ 0.0f };
    float w{ 1.0f };
};

struct Vertex
{
    Vec3 position;
    Vec3 normal{ 0.0f, 0.0f, 0.0f };
    Vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
};
#pragma pack(pop)

int main(int argc, char* argv[])
{
    namespace fs = std::filesystem;

    const fs::path baseDir = fs::path("resources/environment/terrain");
    const fs::path defaultSource = fs::exists(baseDir / "sedbergh_full.mesh")
        ? (baseDir / "sedbergh_full.mesh")
        : (baseDir / "garsdale.mesh");
    const fs::path inputPath  = (argc > 1) ? fs::path(argv[1]) : defaultSource;
    const fs::path backupPath = inputPath.parent_path() / "sedbergh_full.mesh";
    const fs::path outputPath = (argc > 2) ? fs::path(argv[2]) : (baseDir / "garsdale.mesh");

    std::cout << "[CropMesh] Source mesh file: " << inputPath.string() << "\n";
    std::cout << "[CropMesh] Target output:    " << outputPath.string() << "\n";

    if (!fs::exists(inputPath))
    {
        std::cerr << "[CropMesh] Error: Input file does not exist: " << inputPath.string() << "\n";
        return 1;
    }

    std::ifstream inFile(inputPath, std::ios::binary);
    if (!inFile.is_open())
    {
        std::cerr << "[CropMesh] Error: Failed to open " << inputPath.string() << "\n";
        return 1;
    }

    MeshFileHeader header{};
    inFile.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!inFile || std::strncmp(header.magic, "S3DM", 4) != 0)
    {
        std::cerr << "[CropMesh] Error: Invalid S3DM binary mesh header.\n";
        return 1;
    }

    std::cout << "[CropMesh] Header magic: " << std::string(header.magic, 4)
              << ", version: " << header.version
              << ", vertex count: " << header.vertexCount
              << ", index count: " << header.indexCount
              << ", dimensions: " << header.width << "m x " << header.depth << "m\n";

    if (header.vertexCount == 500000u)
    {
        std::cout << "[CropMesh] Notice: " << inputPath.filename().string()
                  << " is already cropped to 500,000 vertices (north half).\n";
        return 0;
    }

    if (header.vertexCount != 1000000u)
    {
        std::cerr << "[CropMesh] Error: Unexpected vertex count: " << header.vertexCount
                  << " (expected 1,000,000 for full 15km dataset).\n";
        return 1;
    }

    // Read full vertex buffer
    std::vector<Vertex> fullVertices(header.vertexCount);
    inFile.read(reinterpret_cast<char*>(fullVertices.data()),
                static_cast<std::streamsize>(header.vertexCount * sizeof(Vertex)));
    if (!inFile)
    {
        std::cerr << "[CropMesh] Error: Failed to read vertices.\n";
        return 1;
    }
    inFile.close();

    // Create backup of full mesh before replacing if input is not already the backup
    if (!fs::exists(backupPath) && inputPath != backupPath)
    {
        std::cout << "[CropMesh] Creating full 15km backup: " << backupPath.string() << "...\n";
        fs::copy_file(inputPath, backupPath, fs::copy_options::overwrite_existing);
    }

    // Extract northern half (identical algorithm to TerrainMesh::ExtractNorthHalf)
    constexpr uint32_t resX = 1000u;
    constexpr uint32_t resZ = 1000u;
    constexpr uint32_t zStart = resZ / 2u; // Row 500
    constexpr uint32_t subResX = resX;
    constexpr uint32_t subResZ = resZ - zStart; // 500 rows
    constexpr uint32_t subVertexCount = subResX * subResZ;

    const float zMinLocal = fullVertices[zStart * resX].position.z;
    const float zMaxLocal = fullVertices[(resZ - 1u) * resX].position.z;
    constexpr float halfFactor = 0.5f;
    const float zCenterOffset = (zMinLocal + zMaxLocal) * halfFactor;

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
            Vertex v = fullVertices[rowOffset + ix];
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

    // Generate triangle indices (clockwise winding for DirectX 12 left-handed space)
    constexpr uint32_t quadCountX = subResX - 1u;
    constexpr uint32_t quadCountZ = subResZ - 1u;
    constexpr uint32_t trianglesPerQuad = 2u;
    constexpr uint32_t indicesPerTriangle = 3u;
    constexpr uint32_t subTriangleCount = quadCountX * quadCountZ * trianglesPerQuad;
    constexpr uint32_t subIndexCount = subTriangleCount * indicesPerTriangle;

    std::vector<uint32_t> subIndices;
    subIndices.reserve(subIndexCount);

    for (uint32_t iz = 0; iz < quadCountZ; ++iz)
    {
        const uint32_t row0 = iz * subResX;
        const uint32_t row1 = (iz + 1u) * subResX;

        for (uint32_t ix = 0; ix < quadCountX; ++ix)
        {
            const uint32_t i0 = row0 + ix;
            const uint32_t i1 = row0 + ix + 1u;
            const uint32_t i2 = row1 + ix;
            const uint32_t i3 = row1 + ix + 1u;

            // Triangle 1: i0 -> i2 -> i3
            subIndices.push_back(i0);
            subIndices.push_back(i2);
            subIndices.push_back(i3);

            // Triangle 2: i0 -> i3 -> i1
            subIndices.push_back(i0);
            subIndices.push_back(i3);
            subIndices.push_back(i1);
        }
    }

    // Prepare updated header for the north half mesh
    MeshFileHeader subHeader = header;
    subHeader.vertexCount  = subVertexCount;
    subHeader.indexCount   = subIndexCount;
    subHeader.minX         = minX;
    subHeader.maxX         = maxX;
    subHeader.minY         = minY;
    subHeader.maxY         = maxY;
    subHeader.minZ         = minZ;
    subHeader.maxZ         = maxZ;
    subHeader.width        = header.width;
    constexpr double subDepthRatio = 0.5;
    subHeader.depth        = header.depth * subDepthRatio;
    subHeader.minElevation = minY;
    subHeader.maxElevation = maxY;

    // Write cropped north half binary mesh
    std::ofstream outFile(outputPath, std::ios::binary | std::ios::trunc);
    if (!outFile.is_open())
    {
        std::cerr << "[CropMesh] Error: Failed to open output file for writing: " << outputPath.string() << "\n";
        return 1;
    }

    outFile.write(reinterpret_cast<const char*>(&subHeader), sizeof(subHeader));
    outFile.write(reinterpret_cast<const char*>(subVertices.data()),
                  static_cast<std::streamsize>(subVertices.size() * sizeof(Vertex)));
    outFile.write(reinterpret_cast<const char*>(subIndices.data()),
                  static_cast<std::streamsize>(subIndices.size() * sizeof(uint32_t)));

    outFile.close();

    const auto newFileSize = fs::file_size(outputPath);
    std::cout << "[CropMesh] Successfully exported north-half mesh to: " << outputPath.string() << "\n";
    std::cout << "           Vertices: " << subHeader.vertexCount << "\n";
    std::cout << "           Indices:  " << subHeader.indexCount << "\n";
    std::cout << "           Bounds:   [" << subHeader.minX << ", " << subHeader.minY << ", " << subHeader.minZ
              << "] to [" << subHeader.maxX << ", " << subHeader.maxY << ", " << subHeader.maxZ << "]\n";
    std::cout << "           Elevation: " << subHeader.minElevation << "m to " << subHeader.maxElevation << "m\n";
    std::cout << "           Depth:     " << subHeader.depth << "m (15km x 7.5km)\n";
    std::cout << "           File Size: " << newFileSize << " bytes (" << (newFileSize / (1024 * 1024)) << " MB)\n";

    return 0;
}

