// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "TerrainObject.h"
#include "TerrainCollider.h"
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

        // Attach default procedural terrain collider based on continuous generator
        const double minElev = -config.heightScale;
        const double maxElev = config.heightScale * 2.0;
        auto collider = std::make_shared<TerrainCollider>(
            config.width,
            config.depth,
            [this](double x, double z) { return m_generator.GenerateHeight(x, z); },
            minElev,
            maxElev
        );
        SetCollider(std::move(collider));
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

        const double minElev = -config.heightScale;
        const double maxElev = config.heightScale * 2.0;

        // Reconfigure procedural collider if not already backed by an elevation grid
        if (auto terrainCol = GetTerrainCollider())
        {
            if (!terrainCol->HasElevationGrid())
            {
                terrainCol->SetHeightSampler(
                    [this](double x, double z) { return m_generator.GenerateHeight(x, z); },
                    config.width,
                    config.depth,
                    minElev,
                    maxElev
                );
            }
        }
        else
        {
            auto collider = std::make_shared<TerrainCollider>(
                config.width,
                config.depth,
                [this](double x, double z) { return m_generator.GenerateHeight(x, z); },
                minElev,
                maxElev
            );
            SetCollider(std::move(collider));
        }
    }

    double TerrainObject::GetHeightAt(double worldX, double worldZ) const noexcept
    {
        if (auto terrainCol = GetTerrainCollider())
        {
            return terrainCol->GetHeightAt(worldX, worldZ);
        }
        return m_generator.GenerateHeight(worldX, worldZ);
    }

    std::shared_ptr<TerrainCollider> TerrainObject::GetTerrainCollider() const noexcept
    {
        return std::dynamic_pointer_cast<TerrainCollider>(m_collider);
    }
}

