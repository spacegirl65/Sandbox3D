// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "TerrainObject.h"
#include "TerrainMesh.h"

namespace Sandbox3D::Engine
{
    TerrainObject::TerrainObject(const Terrain::TerrainConfig& config, std::string_view name)
        : Body(name)
        , m_config(config)
        , m_generator(config)
    {
        SetPosition(m_config.origin);
    }

    TerrainObject::TerrainObject(ID3D12Device* device, const Terrain::TerrainConfig& config, std::string_view name)
        : TerrainObject(config, name)
    {
        Initialise(device);
    }

    void TerrainObject::Update([[maybe_unused]] float deltaTime)
    {
    }

    void TerrainObject::Initialise(ID3D12Device* device)
    {
        if (!device)
        {
            return;
        }

        // Build indexed terrain mesh across configured bounds
        auto mesh = Terrain::TerrainMesh::Build(
            device,
            m_generator,
            m_config.width,
            m_config.depth,
            m_config.resolutionX,
            m_config.resolutionZ,
            m_config.origin
        );

        if (mesh)
        {
            SetMesh(std::move(mesh));
            SetPosition(m_config.origin);
        }
    }

    void TerrainObject::Rebuild(ID3D12Device* device, const Terrain::TerrainConfig& config)
    {
        m_config = config;
        m_generator.SetConfig(config);
        Initialise(device);
    }

    double TerrainObject::GetHeightAt(double worldX, double worldZ) const noexcept
    {
        return m_generator.GenerateHeight(worldX, worldZ);
    }
}

