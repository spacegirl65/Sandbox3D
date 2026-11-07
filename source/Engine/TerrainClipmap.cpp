// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "TerrainClipmap.h"
#include "LightChannel.h"

#include <algorithm>
#include <cmath>

namespace Sandbox3D::Engine
{
    bool TerrainClipmap::Initialise(
        ID3D12Device* device,
        uint32_t patchQuads,
        uint32_t maxLodLevels,
        double terrainMinX,
        double terrainMaxX,
        double terrainMinZ,
        double terrainMaxZ,
        double centerElevation,
        std::span<const float> elevations,
        uint32_t resX,
        uint32_t resZ
    )
    {
        if (!device || patchQuads < 4 || (patchQuads % 2) != 0 || maxLodLevels == 0)
        {
            return false;
        }

        m_patchQuads      = patchQuads;
        m_maxLodLevels    = maxLodLevels;
        m_terrainMinX     = terrainMinX;
        m_terrainMaxX     = terrainMaxX;
        m_terrainMinZ     = terrainMinZ;
        m_terrainMaxZ     = terrainMaxZ;
        m_centerElevation = centerElevation;
        m_resX            = resX;
        m_resZ            = resZ;
        m_elevations.assign(elevations.begin(), elevations.end());

        // Create reusable single-patch template mesh on GPU spanning [0, patchQuads] x [0, patchQuads] with perimeter skirts
        m_patchMesh = CreatePatchMesh(device, m_patchQuads);
        if (!m_patchMesh)
        {
            return false;
        }

        // Establish 16 x 8 square root tiles spanning 15000m x 7500m (each root tile is 937.5m x 937.5m)
        m_rootTilesX = 16u;
        m_rootTilesZ = 8u;

        // Calibrated 3D distance thresholds maintaining crisp ridgeline silhouettes and eliminating sub-pixel slivers
        m_lodRanges.clear();
        m_lodRanges.reserve(m_maxLodLevels);
        m_lodRanges.push_back(1500.0); // LOD 0 (937.5m patch, 14.65m spacing) subdivides into LOD 1 if distance < 1500m (crisp mid-distance silhouettes)
        m_lodRanges.push_back(250.0); // LOD 1 (468.75m patch, 7.32m spacing) subdivides into LOD 2 if distance < 250m
        m_lodRanges.push_back(80.0);  // LOD 2 (234.38m patch, 3.66m spacing) subdivides into LOD 3 if distance < 80m
        m_lodRanges.push_back(25.0);  // LOD 3 (117.19m patch, 1.83m spacing) subdivides into LOD 4 if distance < 25m (prevents sub-pixel slivers)

        m_activePatches.reserve(128);
        m_isInitialised = true;
        return true;
    }

    void TerrainClipmap::Update(
        const Maths::Vec3D& cameraPosition,
        const Maths::BoundingFrustumD& frustum
    ) noexcept
    {
        if (!m_isInitialised)
        {
            return;
        }

        m_activePatches.clear();

        const double terrainWidth  = m_terrainMaxX - m_terrainMinX;
        const double terrainDepth  = m_terrainMaxZ - m_terrainMinZ;
        const double rootTileSizeX = terrainWidth / static_cast<double>(m_rootTilesX);
        const double rootTileSizeZ = terrainDepth / static_cast<double>(m_rootTilesZ);

        for (uint32_t tz = 0; tz < m_rootTilesZ; ++tz)
        {
            const double nodeMinZ = m_terrainMinZ + static_cast<double>(tz) * rootTileSizeZ;
            for (uint32_t tx = 0; tx < m_rootTilesX; ++tx)
            {
                const double nodeMinX = m_terrainMinX + static_cast<double>(tx) * rootTileSizeX;
                SelectPatchesRecursive(nodeMinX, nodeMinZ, rootTileSizeX, rootTileSizeZ, 0u, cameraPosition, &frustum);
            }
        }
    }

    void TerrainClipmap::Update(const Maths::Vec3D& cameraPosition) noexcept
    {
        if (!m_isInitialised)
        {
            return;
        }

        m_activePatches.clear();

        const double terrainWidth  = m_terrainMaxX - m_terrainMinX;
        const double terrainDepth  = m_terrainMaxZ - m_terrainMinZ;
        const double rootTileSizeX = terrainWidth / static_cast<double>(m_rootTilesX);
        const double rootTileSizeZ = terrainDepth / static_cast<double>(m_rootTilesZ);

        for (uint32_t tz = 0; tz < m_rootTilesZ; ++tz)
        {
            const double nodeMinZ = m_terrainMinZ + static_cast<double>(tz) * rootTileSizeZ;
            for (uint32_t tx = 0; tx < m_rootTilesX; ++tx)
            {
                const double nodeMinX = m_terrainMinX + static_cast<double>(tx) * rootTileSizeX;
                SelectPatchesRecursive(nodeMinX, nodeMinZ, rootTileSizeX, rootTileSizeZ, 0u, cameraPosition, nullptr);
            }
        }
    }

    double TerrainClipmap::SampleElevation(double worldX, double worldZ) const noexcept
    {
        if (m_elevations.empty() || m_resX == 0 || m_resZ == 0)
        {
            return 0.0;
        }

        const double normX = std::clamp((worldX - m_terrainMinX) / (m_terrainMaxX - m_terrainMinX), 0.0, 1.0);
        const double normZ = std::clamp((worldZ - m_terrainMinZ) / (m_terrainMaxZ - m_terrainMinZ), 0.0, 1.0);
        const uint32_t col = std::min(static_cast<uint32_t>(normX * static_cast<double>(m_resX - 1u)), m_resX - 1u);
        const uint32_t row = std::min(static_cast<uint32_t>(normZ * static_cast<double>(m_resZ - 1u)), m_resZ - 1u);
        return static_cast<double>(m_elevations[row * m_resX + col]) - m_centerElevation;
    }

    void TerrainClipmap::SelectPatchesRecursive(
        double minX,
        double minZ,
        double sizeX,
        double sizeZ,
        uint32_t lodLevel,
        const Maths::Vec3D& cameraPosition,
        const Maths::BoundingFrustumD* frustum
    ) noexcept
    {
        const double maxX = minX + sizeX;
        const double maxZ = minZ + sizeZ;

        constexpr double minElevation = 80.0;
        constexpr double maxElevation = 700.0;
        constexpr double elevationPadding = 40.0;
        const Maths::BoundingBoxD worldBox(
            Maths::Vec3D(minX, minElevation - m_centerElevation - elevationPadding, minZ),
            Maths::Vec3D(maxX, maxElevation - m_centerElevation + elevationPadding, maxZ)
        );

        // Early-out frustum culling: discard patches outside camera field of view before subdivision
        if (frustum && !frustum->Intersects(worldBox))
        {
            return;
        }

        // Compute 3D Euclidean distance from camera position to the closest surface point of the patch
        const double clampedX = std::clamp(cameraPosition.x, minX, maxX);
        const double clampedZ = std::clamp(cameraPosition.z, minZ, maxZ);
        const double surfaceY = SampleElevation(clampedX, clampedZ);
        const double deltaX   = cameraPosition.x - clampedX;
        const double deltaY   = cameraPosition.y - surfaceY;
        const double deltaZ   = cameraPosition.z - clampedZ;
        const double distSq   = deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;

        const bool canSubdivide = (lodLevel < (m_maxLodLevels - 1u)) && (lodLevel < m_lodRanges.size());
        const bool shouldSubdivide = canSubdivide && (distSq < (m_lodRanges[lodLevel] * m_lodRanges[lodLevel]));

        if (shouldSubdivide)
        {
            const double halfSizeX = sizeX * 0.5;
            const double halfSizeZ = sizeZ * 0.5;
            const uint32_t nextLod = lodLevel + 1u;

            SelectPatchesRecursive(minX, minZ, halfSizeX, halfSizeZ, nextLod, cameraPosition, frustum);
            SelectPatchesRecursive(minX + halfSizeX, minZ, halfSizeX, halfSizeZ, nextLod, cameraPosition, frustum);
            SelectPatchesRecursive(minX, minZ + halfSizeZ, halfSizeX, halfSizeZ, nextLod, cameraPosition, frustum);
            SelectPatchesRecursive(minX + halfSizeX, minZ + halfSizeZ, halfSizeX, halfSizeZ, nextLod, cameraPosition, frustum);
        }
        else
        {
            // Select terminal leaf patch and compute camera-relative world transformation
            const double spacingX = sizeX / static_cast<double>(m_patchQuads);
            const double spacingZ = sizeZ / static_cast<double>(m_patchQuads);

            const Maths::Mat4x4D scaleMatrix = Maths::Mat4x4D::Scale(spacingX, 1.0, spacingZ);
            const Maths::Mat4x4D transMatrix = Maths::Mat4x4D::Translation(minX, -m_centerElevation, minZ);

            TerrainPatchInstance patch{};
            patch.worldMatrix = scaleMatrix * transMatrix;
            patch.colorTint   = Maths::Vec4(
                static_cast<float>(minX),
                static_cast<float>(minZ),
                static_cast<float>(spacingX),
                -1.0f
            );

            if (lodLevel > 0u && (lodLevel - 1u) < m_lodRanges.size())
            {
                constexpr float morphRatio = 0.30f;
                const float morphEnd   = static_cast<float>(m_lodRanges[lodLevel - 1u]);
                const float morphStart = morphEnd * (1.0f - morphRatio);
                const float invRange   = 1.0f / (morphEnd - morphStart);
                patch.morphParams      = Maths::Vec4(morphStart, morphEnd, invRange, static_cast<float>(lodLevel));
            }
            else
            {
                // Root coarsest patches do not undergo geomorphing
                patch.morphParams = Maths::Vec4(1e9f, 1e9f, 0.0f, 0.0f);
            }

            m_activePatches.push_back(patch);
        }
    }

    void TerrainClipmap::GenerateRenderItems(
        std::shared_ptr<Renderer::Material> terrainMaterial,
        std::vector<Renderer::RenderItem>& outItems
    ) const
    {
        if (!m_isInitialised || !m_patchMesh || m_activePatches.empty())
        {
            return;
        }

        outItems.reserve(outItems.size() + m_activePatches.size());

        for (const auto& patch : m_activePatches)
        {
            Renderer::RenderItem item{};
            item.mesh          = m_patchMesh;
            item.material      = terrainMaterial;
            item.worldMatrix   = patch.worldMatrix;
            item.colorTint     = patch.colorTint;
            item.morphParams   = patch.morphParams;
            item.lightChannels = LightChannel::Terrain;
            item.isVisible     = true;
            item.name          = "TerrainPatch";

            outItems.push_back(std::move(item));
        }
    }

    std::shared_ptr<Renderer::Mesh> TerrainClipmap::CreatePatchMesh(
        ID3D12Device* device,
        uint32_t patchQuads
    )
    {
        const uint32_t vertsAcross = patchQuads + 1u;
        const size_t gridVertices = static_cast<size_t>(vertsAcross) * vertsAcross;
        const size_t skirtVerticesCount = 4u * vertsAcross;

        std::vector<Renderer::Vertex> vertices;
        vertices.reserve(gridVertices + skirtVerticesCount);

        // Surface terrain vertices (y = 0.0f)
        for (uint32_t iz = 0; iz < vertsAcross; ++iz)
        {
            const float z = static_cast<float>(iz);
            for (uint32_t ix = 0; ix < vertsAcross; ++ix)
            {
                const float x = static_cast<float>(ix);
                Renderer::Vertex v{};
                v.position = Maths::Vec3(x, 0.0f, z);
                v.normal   = Maths::Vec3(0.0f, 1.0f, 0.0f);
                v.color    = Maths::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
                vertices.push_back(v);
            }
        }

        // Alternating checkerboard diagonal triangulation
        const size_t totalQuads = static_cast<size_t>(patchQuads) * patchQuads;
        std::vector<uint32_t> indices;
        indices.reserve((totalQuads + 4u * patchQuads) * 6u);

        for (uint32_t iz = 0; iz < patchQuads; ++iz)
        {
            for (uint32_t ix = 0; ix < patchQuads; ++ix)
            {
                const uint32_t v00 = iz * vertsAcross + ix;
                const uint32_t v10 = iz * vertsAcross + (ix + 1u);
                const uint32_t v01 = (iz + 1u) * vertsAcross + ix;
                const uint32_t v11 = (iz + 1u) * vertsAcross + (ix + 1u);

                if (((ix + iz) & 1u) == 0u)
                {
                    indices.push_back(v00);
                    indices.push_back(v01);
                    indices.push_back(v11);

                    indices.push_back(v00);
                    indices.push_back(v11);
                    indices.push_back(v10);
                }
                else
                {
                    indices.push_back(v00);
                    indices.push_back(v01);
                    indices.push_back(v10);

                    indices.push_back(v10);
                    indices.push_back(v01);
                    indices.push_back(v11);
                }
            }
        }

        // Perimeter skirts dropping vertically to eliminate sub-pixel rasterizer discrepancies completely
        // South border (iz = 0): looking towards +Z
        const uint32_t southSkirtBase = static_cast<uint32_t>(vertices.size());
        for (uint32_t ix = 0; ix < vertsAcross; ++ix)
        {
            Renderer::Vertex v{};
            v.position = Maths::Vec3(static_cast<float>(ix), -1.0f, 0.0f);
            v.normal   = Maths::Vec3(0.0f, 0.0f, -1.0f);
            v.color    = Maths::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
            vertices.push_back(v);
        }
        for (uint32_t ix = 0; ix < patchQuads; ++ix)
        {
            const uint32_t top0 = ix;
            const uint32_t top1 = ix + 1u;
            const uint32_t bot0 = southSkirtBase + ix;
            const uint32_t bot1 = southSkirtBase + ix + 1u;

            indices.push_back(top0);
            indices.push_back(top1);
            indices.push_back(bot1);

            indices.push_back(top0);
            indices.push_back(bot1);
            indices.push_back(bot0);
        }

        // North border (iz = patchQuads): looking towards -Z
        const uint32_t northSkirtBase = static_cast<uint32_t>(vertices.size());
        const uint32_t northRow = patchQuads * vertsAcross;
        for (uint32_t ix = 0; ix < vertsAcross; ++ix)
        {
            Renderer::Vertex v{};
            v.position = Maths::Vec3(static_cast<float>(ix), -1.0f, static_cast<float>(patchQuads));
            v.normal   = Maths::Vec3(0.0f, 0.0f, 1.0f);
            v.color    = Maths::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
            vertices.push_back(v);
        }
        for (uint32_t ix = 0; ix < patchQuads; ++ix)
        {
            const uint32_t top0 = northRow + ix;
            const uint32_t top1 = northRow + ix + 1u;
            const uint32_t bot0 = northSkirtBase + ix;
            const uint32_t bot1 = northSkirtBase + ix + 1u;

            indices.push_back(top1);
            indices.push_back(top0);
            indices.push_back(bot0);

            indices.push_back(top1);
            indices.push_back(bot0);
            indices.push_back(bot1);
        }

        // West border (ix = 0): looking towards +X
        const uint32_t westSkirtBase = static_cast<uint32_t>(vertices.size());
        for (uint32_t iz = 0; iz < vertsAcross; ++iz)
        {
            Renderer::Vertex v{};
            v.position = Maths::Vec3(0.0f, -1.0f, static_cast<float>(iz));
            v.normal   = Maths::Vec3(-1.0f, 0.0f, 0.0f);
            v.color    = Maths::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
            vertices.push_back(v);
        }
        for (uint32_t iz = 0; iz < patchQuads; ++iz)
        {
            const uint32_t top0 = iz * vertsAcross;
            const uint32_t top1 = (iz + 1u) * vertsAcross;
            const uint32_t bot0 = westSkirtBase + iz;
            const uint32_t bot1 = westSkirtBase + iz + 1u;

            indices.push_back(top1);
            indices.push_back(top0);
            indices.push_back(bot0);

            indices.push_back(top1);
            indices.push_back(bot0);
            indices.push_back(bot1);
        }

        // East border (ix = patchQuads): looking towards -X
        const uint32_t eastSkirtBase = static_cast<uint32_t>(vertices.size());
        for (uint32_t iz = 0; iz < vertsAcross; ++iz)
        {
            Renderer::Vertex v{};
            v.position = Maths::Vec3(static_cast<float>(patchQuads), -1.0f, static_cast<float>(iz));
            v.normal   = Maths::Vec3(1.0f, 0.0f, 0.0f);
            v.color    = Maths::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
            vertices.push_back(v);
        }
        for (uint32_t iz = 0; iz < patchQuads; ++iz)
        {
            const uint32_t top0 = iz * vertsAcross + patchQuads;
            const uint32_t top1 = (iz + 1u) * vertsAcross + patchQuads;
            const uint32_t bot0 = eastSkirtBase + iz;
            const uint32_t bot1 = eastSkirtBase + iz + 1u;

            indices.push_back(top0);
            indices.push_back(top1);
            indices.push_back(bot1);

            indices.push_back(top0);
            indices.push_back(bot1);
            indices.push_back(bot0);
        }

        auto mesh = std::make_shared<Renderer::Mesh>();
        mesh->Initialise(device, vertices, indices);

        constexpr float maxTerrainElevation = 1200.0f;
        mesh->SetBoundingBox(Maths::BoundingBox(
            Maths::Vec3(0.0f, -5.0f, 0.0f),
            Maths::Vec3(static_cast<float>(patchQuads), maxTerrainElevation, static_cast<float>(patchQuads))
        ));

        const float halfQuads = static_cast<float>(patchQuads) * 0.5f;
        const float radius = std::sqrt(halfQuads * halfQuads * 2.0f + 650.0f * 650.0f);
        mesh->SetBoundingSphere(Maths::BoundingSphere(
            Maths::Vec3(halfQuads, 575.0f, halfQuads),
            radius
        ));

        return mesh;
    }
}
