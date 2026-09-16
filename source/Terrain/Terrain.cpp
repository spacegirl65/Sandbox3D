// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Terrain.h"

namespace Sandbox3D::Terrain
{
    Terrain::Terrain(const TerrainConfig& config)
        : m_config(config)
        , m_generator(config)
    {
    }

    void Terrain::Initialise(ID3D12Device* device)
    {
        if (!device)
        {
            return;
        }

        m_renderItems.clear();

        // Build indexed terrain mesh across the configured area bounds
        m_mesh = TerrainMesh::Build(
            device,
            m_generator,
            m_config.width,
            m_config.depth,
            m_config.resolutionX,
            m_config.resolutionZ,
            m_config.origin
        );

        if (m_mesh)
        {
            Renderer::RenderItem item;
            item.mesh = m_mesh;
            item.worldMatrix = Maths::Mat4x4D::Translation(m_config.origin);
            item.isVisible = true;
            item.name = "Terrain";

            m_renderItems.push_back(std::move(item));
        }
    }

    void Terrain::Rebuild(ID3D12Device* device, const TerrainConfig& config)
    {
        m_config = config;
        m_generator.SetConfig(config);
        Initialise(device);
    }

    double Terrain::GetHeightAt(double worldX, double worldZ) const noexcept
    {
        return m_generator.GenerateHeight(worldX, worldZ);
    }
}

