// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "TerrainObject.h"
#include "Renderer/Material.h"
#include "TerrainMesh.h"

namespace Sandbox3D::Engine
{
    TerrainObject::TerrainObject(const TerrainConfig& config, std::string_view name)
        : Body(name)
        , m_config(config)
        , m_generator(config)
    {
        SetMaterial(Material::CreateTerrain());
        SetPosition(m_config.origin);
    }

    TerrainObject::TerrainObject(ID3D12Device* device, const TerrainConfig& config, std::string_view name)
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

        // 1. Attempt to load pre-processed LiDAR DTM heightmap if available
        std::shared_ptr<Renderer::Mesh> mesh = TerrainMesh::BuildFromFile(
            device,
            "resources/environment/terrain/terrain_15km.bin",
            m_config.origin
        );

        // 2. Fall back to continuous procedural terrain generation
        if (!mesh)
        {
            mesh = TerrainMesh::Build(
                device,
                m_generator,
                m_config.width,
                m_config.depth,
                m_config.resolutionX,
                m_config.resolutionZ,
                m_config.origin
            );
        }

        if (mesh)
        {
            SetMesh(std::move(mesh));
            SetPosition(m_config.origin);
        }
    }

    void TerrainObject::Rebuild(ID3D12Device* device, const TerrainConfig& config)
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

