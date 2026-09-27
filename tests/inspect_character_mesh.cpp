#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <cstdint>
#include "Renderer/Mesh.h"

using namespace Sandbox3D;
using namespace Sandbox3D::Renderer;

int main()
{
    std::ifstream file("resources/entities/character.mesh", std::ios::binary);
    if (!file.is_open())
    {
        std::cout << "Cannot open character.mesh\n";
        return 1;
    }

    MeshFileHeader header{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    std::cout << "Magic: " << header.magic[0] << header.magic[1] << header.magic[2] << header.magic[3] << "\n";
    std::cout << "Vertices: " << header.vertexCount << "\n";
    std::cout << "Indices: " << header.indexCount << "\n";
    std::cout << "Bounds: [" << header.minX << ", " << header.minY << ", " << header.minZ << "] to ["
              << header.maxX << ", " << header.maxY << ", " << header.maxZ << "]\n";

    std::vector<Vertex> vertices(header.vertexCount);
    file.read(reinterpret_cast<char*>(vertices.data()), header.vertexCount * sizeof(Vertex));

    // Analyze Y distribution of vertices
    float minY = 1e9f, maxY = -1e9f;
    for (const auto& v : vertices)
    {
        minY = std::min(minY, v.position.y);
        maxY = std::max(maxY, v.position.y);
    }
    std::cout << "Min Y: " << minY << ", Max Y: " << maxY << "\n";

    // Print first 20 vertices and their normals
    std::cout << "\n--- Vertices sample ---\n";
    for (size_t i = 0; i < 20; ++i)
    {
        std::cout << "v[" << i << "]: pos=(" << vertices[i].position.x << ", " << vertices[i].position.y << ", " << vertices[i].position.z
                  << ") norm=(" << vertices[i].normal.x << ", " << vertices[i].normal.y << ", " << vertices[i].normal.z << ")\n";
    }

    // Print bottom vertices (lowest Y)
    std::cout << "\n--- Lowest Y vertices ---\n";
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        if (vertices[i].position.y < minY + 0.1f)
        {
            std::cout << "v[" << i << "]: pos=(" << vertices[i].position.x << ", " << vertices[i].position.y << ", " << vertices[i].position.z
                      << ") norm=(" << vertices[i].normal.x << ", " << vertices[i].normal.y << ", " << vertices[i].normal.z << ")\n";
        }
    }

    std::vector<uint32_t> indices(header.indexCount);
    file.read(reinterpret_cast<char*>(indices.data()), header.indexCount * sizeof(uint32_t));

    size_t frontCount = 0;
    size_t backCount = 0;
    size_t triCount = indices.size() / 3;

    for (size_t t = 0; t < triCount; ++t)
    {
        uint32_t i0 = indices[t * 3 + 0];
        uint32_t i1 = indices[t * 3 + 1];
        uint32_t i2 = indices[t * 3 + 2];

        const Vec3& v0 = vertices[i0].position;
        const Vec3& v1 = vertices[i1].position;
        const Vec3& v2 = vertices[i2].position;

        float e1x = v1.x - v0.x, e1y = v1.y - v0.y, e1z = v1.z - v0.z;
        float e2x = v2.x - v0.x, e2y = v2.y - v0.y, e2z = v2.z - v0.z;

        float nx = e1y * e2z - e1z * e2y;
        float ny = e1z * e2x - e1x * e2z;
        float nz = e1x * e2y - e1y * e2x;

        float vnx = vertices[i0].normal.x + vertices[i1].normal.x + vertices[i2].normal.x;
        float vny = vertices[i0].normal.y + vertices[i1].normal.y + vertices[i2].normal.y;
        float vnz = vertices[i0].normal.z + vertices[i1].normal.z + vertices[i2].normal.z;

        float dot = nx * vnx + ny * vny + nz * vnz;
        if (dot > 0.0f)
        {
            ++frontCount;
        }
        else
        {
            ++backCount;
        }
    }

    std::cout << "Total triangles: " << triCount << "\n";
    std::cout << "Outward (front) triangles: " << frontCount << "\n";
    std::cout << "Inward (back) triangles: " << backCount << "\n";

    return 0;
}
