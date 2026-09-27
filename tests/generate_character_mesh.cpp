#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <cassert>

#include "Renderer/Mesh.h"
#include "Maths/Vec3.h"
#include "Maths/Vec4.h"

using namespace Sandbox3D;
using namespace Sandbox3D::Renderer;
using namespace Sandbox3D::Maths;

int main()
{
    std::cout << "Generating character.mesh with corrected clockwise winding...\n";

    constexpr float capsuleRadius = 0.35f;
    constexpr float capsuleCylinderHeight = 0.70f;
    constexpr float headRadius = 0.22f;
    const Vec4 bodyColor = Vec4::White();
    const Vec4 headColor = Vec4::White();

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    constexpr uint32_t sliceCount = 24u;
    constexpr uint32_t halfStacks = 8u;
    constexpr float pi = std::numbers::pi_v<float>;
    constexpr float twoPi = 2.0f * pi;

    // =====================================================================
    // Part 1: Capsule Body (Base at Y = 0.0, Top at Y = 2*r + H)
    // =====================================================================
    const float r = capsuleRadius;
    const float h = capsuleCylinderHeight;
    const float topHemisphereCenterY    = r + h;
    const float bottomHemisphereCenterY = r;

    // Top pole of capsule body
    const uint32_t bodyTopPoleIdx = static_cast<uint32_t>(vertices.size());
    vertices.push_back({
        Vec3(0.0f, topHemisphereCenterY + r, 0.0f),
        Vec3(0.0f, 1.0f, 0.0f),
        bodyColor
    });

    // Top hemisphere rings (excluding pole and equator)
    for (uint32_t i = 1; i < halfStacks; ++i)
    {
        const float phi = (static_cast<float>(i) / static_cast<float>(halfStacks)) * (pi * 0.5f);
        const float cosPhi = std::cos(phi);
        const float sinPhi = std::sin(phi);
        const float y = topHemisphereCenterY + r * cosPhi;
        const float ringR = r * sinPhi;

        for (uint32_t j = 0; j < sliceCount; ++j)
        {
            const float theta = (static_cast<float>(j) / static_cast<float>(sliceCount)) * twoPi;
            const float cosTheta = std::cos(theta);
            const float sinTheta = std::sin(theta);

            const Vec3 normal(sinPhi * sinTheta, cosPhi, sinPhi * cosTheta);
            const Vec3 position(ringR * sinTheta, y, ringR * cosTheta);
            vertices.push_back({ position, normal.Normalised(), bodyColor });
        }
    }

    // Top cylinder ring (at top of cylinder)
    const uint32_t topCylinderRingStart = static_cast<uint32_t>(vertices.size());
    for (uint32_t j = 0; j < sliceCount; ++j)
    {
        const float theta = (static_cast<float>(j) / static_cast<float>(sliceCount)) * twoPi;
        const float cosTheta = std::cos(theta);
        const float sinTheta = std::sin(theta);

        const Vec3 normal(sinTheta, 0.0f, cosTheta);
        const Vec3 position(r * sinTheta, topHemisphereCenterY, r * cosTheta);
        vertices.push_back({ position, normal.Normalised(), bodyColor });
    }

    // Bottom cylinder ring (at bottom of cylinder)
    const uint32_t bottomCylinderRingStart = static_cast<uint32_t>(vertices.size());
    for (uint32_t j = 0; j < sliceCount; ++j)
    {
        const float theta = (static_cast<float>(j) / static_cast<float>(sliceCount)) * twoPi;
        const float cosTheta = std::cos(theta);
        const float sinTheta = std::sin(theta);

        const Vec3 normal(sinTheta, 0.0f, cosTheta);
        const Vec3 position(r * sinTheta, bottomHemisphereCenterY, r * cosTheta);
        vertices.push_back({ position, normal.Normalised(), bodyColor });
    }

    // Bottom hemisphere rings (from below equator to above bottom pole)
    for (uint32_t i = 1; i < halfStacks; ++i)
    {
        const float phi = (pi * 0.5f) + (static_cast<float>(i) / static_cast<float>(halfStacks)) * (pi * 0.5f);
        const float cosPhi = std::cos(phi);
        const float sinPhi = std::sin(phi);
        const float y = bottomHemisphereCenterY + r * cosPhi;
        const float ringR = r * sinPhi;

        for (uint32_t j = 0; j < sliceCount; ++j)
        {
            const float theta = (static_cast<float>(j) / static_cast<float>(sliceCount)) * twoPi;
            const float cosTheta = std::cos(theta);
            const float sinTheta = std::sin(theta);

            const Vec3 normal(sinPhi * sinTheta, cosPhi, sinPhi * cosTheta);
            const Vec3 position(ringR * sinTheta, y, ringR * cosTheta);
            vertices.push_back({ position, normal.Normalised(), bodyColor });
        }
    }

    // Bottom pole of capsule body
    const uint32_t bodyBottomPoleIdx = static_cast<uint32_t>(vertices.size());
    vertices.push_back({
        Vec3(0.0f, bottomHemisphereCenterY - r, 0.0f),
        Vec3(0.0f, -1.0f, 0.0f),
        bodyColor
    });

    // Top cap indices (top pole to first ring)
    // Clockwise winding facing outward: bodyTopPoleIdx -> curr -> next
    for (uint32_t j = 0; j < sliceCount; ++j)
    {
        const uint32_t curr = 1u + j;
        const uint32_t next = 1u + ((j + 1u) % sliceCount);
        indices.push_back(bodyTopPoleIdx);
        indices.push_back(curr);
        indices.push_back(next);
    }

    // Intermediate rings indices on capsule body
    // Total ring count = (halfStacks - 1) + 2 (cylinder top and bottom) + (halfStacks - 1)
    const uint32_t totalBodyRings = (halfStacks - 1u) * 2u + 2u;
    for (uint32_t ring = 0; ring < totalBodyRings - 1u; ++ring)
    {
        const uint32_t rowA = 1u + ring * sliceCount;
        const uint32_t rowB = 1u + (ring + 1u) * sliceCount;

        for (uint32_t j = 0; j < sliceCount; ++j)
        {
            const uint32_t nextJ = (j + 1u) % sliceCount;
            const uint32_t a = rowA + j;
            const uint32_t b = rowA + nextJ;
            const uint32_t c = rowB + j;
            const uint32_t d = rowB + nextJ;

            // Clockwise winding facing outward:
            // Triangle 1: a -> d -> b
            indices.push_back(a);
            indices.push_back(d);
            indices.push_back(b);

            // Triangle 2: a -> c -> d
            indices.push_back(a);
            indices.push_back(c);
            indices.push_back(d);
        }
    }

    // Bottom cap indices (last ring to bottom pole)
    // Clockwise winding facing outward: curr -> bodyBottomPoleIdx -> next
    const uint32_t lastBodyRingStart = 1u + (totalBodyRings - 1u) * sliceCount;
    for (uint32_t j = 0; j < sliceCount; ++j)
    {
        const uint32_t curr = lastBodyRingStart + j;
        const uint32_t next = lastBodyRingStart + ((j + 1u) % sliceCount);
        indices.push_back(curr);
        indices.push_back(bodyBottomPoleIdx);
        indices.push_back(next);
    }

    // =====================================================================
    // Part 2: Spherical Head (Centered on top of capsule)
    // =====================================================================
    constexpr float headOverlapFactor = 0.85f;
    const float headCenterY = topHemisphereCenterY + r + headRadius * headOverlapFactor;
    constexpr uint32_t headStackCount = 14u;
    constexpr uint32_t headSliceCount = 24u;

    // Top pole of head
    const uint32_t headTopPoleIdx = static_cast<uint32_t>(vertices.size());
    vertices.push_back({
        Vec3(0.0f, headCenterY + headRadius, 0.0f),
        Vec3(0.0f, 1.0f, 0.0f),
        headColor
    });

    // Intermediate rings of head sphere
    const uint32_t headFirstRingIdx = static_cast<uint32_t>(vertices.size());
    for (uint32_t i = 1; i < headStackCount; ++i)
    {
        const float phi = (static_cast<float>(i) / static_cast<float>(headStackCount)) * pi;
        const float cosPhi = std::cos(phi);
        const float sinPhi = std::sin(phi);
        const float y = headCenterY + headRadius * cosPhi;
        const float ringR = headRadius * sinPhi;

        for (uint32_t j = 0; j < headSliceCount; ++j)
        {
            const float theta = (static_cast<float>(j) / static_cast<float>(headSliceCount)) * twoPi;
            const float cosTheta = std::cos(theta);
            const float sinTheta = std::sin(theta);

            const Vec3 normal(sinPhi * sinTheta, cosPhi, sinPhi * cosTheta);
            const Vec3 position(ringR * sinTheta, y, ringR * cosTheta);
            vertices.push_back({ position, normal.Normalised(), headColor });
        }
    }

    // Bottom pole of head
    const uint32_t headBottomPoleIdx = static_cast<uint32_t>(vertices.size());
    vertices.push_back({
        Vec3(0.0f, headCenterY - headRadius, 0.0f),
        Vec3(0.0f, -1.0f, 0.0f),
        headColor
    });

    // Head top cap
    // Clockwise winding facing outward: headTopPoleIdx -> curr -> next
    for (uint32_t j = 0; j < headSliceCount; ++j)
    {
        const uint32_t curr = headFirstRingIdx + j;
        const uint32_t next = headFirstRingIdx + ((j + 1u) % headSliceCount);
        indices.push_back(headTopPoleIdx);
        indices.push_back(curr);
        indices.push_back(next);
    }

    // Head intermediate quads
    for (uint32_t i = 0; i < headStackCount - 2u; ++i)
    {
        const uint32_t rowA = headFirstRingIdx + i * headSliceCount;
        const uint32_t rowB = headFirstRingIdx + (i + 1u) * headSliceCount;

        for (uint32_t j = 0; j < headSliceCount; ++j)
        {
            const uint32_t nextJ = (j + 1u) % headSliceCount;
            const uint32_t a = rowA + j;
            const uint32_t b = rowA + nextJ;
            const uint32_t c = rowB + j;
            const uint32_t d = rowB + nextJ;

            indices.push_back(a);
            indices.push_back(d);
            indices.push_back(b);

            indices.push_back(a);
            indices.push_back(c);
            indices.push_back(d);
        }
    }

    // Head bottom cap
    // Clockwise winding facing outward: curr -> headBottomPoleIdx -> next
    const uint32_t headLastRowStart = headFirstRingIdx + (headStackCount - 2u) * headSliceCount;
    for (uint32_t j = 0; j < headSliceCount; ++j)
    {
        const uint32_t curr = headLastRowStart + j;
        const uint32_t next = headLastRowStart + ((j + 1u) % headSliceCount);
        indices.push_back(curr);
        indices.push_back(headBottomPoleIdx);
        indices.push_back(next);
    }

    std::cout << "Vertices generated: " << vertices.size() << "\n";
    std::cout << "Indices generated: " << indices.size() << " (" << indices.size() / 3 << " triangles)\n";

    // Verify outward winding for all triangles
    size_t frontCount = 0;
    size_t backCount = 0;
    const size_t triCount = indices.size() / 3;

    for (size_t t = 0; t < triCount; ++t)
    {
        const uint32_t i0 = indices[t * 3 + 0];
        const uint32_t i1 = indices[t * 3 + 1];
        const uint32_t i2 = indices[t * 3 + 2];

        const Vec3& v0 = vertices[i0].position;
        const Vec3& v1 = vertices[i1].position;
        const Vec3& v2 = vertices[i2].position;

        const float e1x = v1.x - v0.x, e1y = v1.y - v0.y, e1z = v1.z - v0.z;
        const float e2x = v2.x - v0.x, e2y = v2.y - v0.y, e2z = v2.z - v0.z;

        const float nx = e1y * e2z - e1z * e2y;
        const float ny = e1z * e2x - e1x * e2z;
        const float nz = e1x * e2y - e1y * e2x;

        const float vnx = vertices[i0].normal.x + vertices[i1].normal.x + vertices[i2].normal.x;
        const float vny = vertices[i0].normal.y + vertices[i1].normal.y + vertices[i2].normal.y;
        const float vnz = vertices[i0].normal.z + vertices[i1].normal.z + vertices[i2].normal.z;

        const float dot = nx * vnx + ny * vny + nz * vnz;
        if (dot > 0.0f)
        {
            ++frontCount;
        }
        else
        {
            ++backCount;
        }
    }

    std::cout << "Outward (front) triangles: " << frontCount << "\n";
    std::cout << "Inward (back) triangles: " << backCount << "\n";

    assert(backCount == 0 && "All triangles must face outward!");
    assert(frontCount == triCount && "Every triangle must be front-facing!");

    // Save to resources/entities/character.mesh
    const bool saved = Mesh::SaveToFile("resources/entities/character.mesh", vertices, indices);
    if (!saved)
    {
        std::cerr << "Failed to save character.mesh!\n";
        return 1;
    }

    std::cout << "Successfully generated and saved resources/entities/character.mesh!\n";
    return 0;
}

