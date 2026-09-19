// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "TerrainObject.h"
#include "Renderer/Material.h"

namespace Sandbox3D::Engine
{
    TerrainObject::TerrainObject(const TerrainConfig& config, std::string_view name)
        : Body(name)
        , m_config(config)
        , m_generator(config)
    {
        SetMaterial(Renderer::Material::CreateTerrain());
        SetPosition(m_config.origin);
    }

    TerrainObject::TerrainObject(std::shared_ptr<Renderer::Mesh> mesh, const TerrainConfig& config, std::string_view name)
        : TerrainObject(config, name)
    {
        if (mesh)
        {
            SetMesh(std::move(mesh));
            SetPosition(m_config.origin);
        }
    }

    void TerrainObject::Update([[maybe_unused]] float deltaTime)
    {
    }

    void TerrainObject::Rebuild(const TerrainConfig& config)
    {
        m_config = config;
        m_generator.SetConfig(config);
        SetPosition(m_config.origin);
    }

    double TerrainObject::GetHeightAt(double worldX, double worldZ) const noexcept
    {
        return m_generator.GenerateHeight(worldX, worldZ);
    }
}

