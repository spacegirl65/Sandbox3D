// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Terrain.h"
#include "Renderer/Material.h"

namespace Sandbox3D::Engine
{
    Terrain::Terrain(const TerrainConfig& config, std::string_view name)
        : Base(name)
        , m_config(config)
        , m_generator(config)
    {
    }

    Terrain::Terrain(std::shared_ptr<Renderer::Mesh> mesh, const TerrainConfig& config, std::string_view name)
        : Terrain(config, name)
    {
        SetMesh(std::move(mesh));
    }

    void Terrain::Update([[maybe_unused]] float deltaTime)
    {
    }

    void Terrain::SetMesh(std::shared_ptr<Renderer::Mesh> mesh)
    {
        m_mesh = std::move(mesh);
        m_renderItems.clear();

        if (m_mesh)
        {
            Renderer::RenderItem item;
            item.mesh        = m_mesh;
            item.material    = Renderer::Material::CreateTerrain();
            item.worldMatrix = Maths::Mat4x4D::Translation(m_config.origin);
            item.isVisible   = true;
            item.name        = GetName();

            m_renderItems.push_back(std::move(item));
        }
    }

    void Terrain::Rebuild(const TerrainConfig& config)
    {
        m_config = config;
        m_generator.SetConfig(config);
        if (m_mesh)
        {
            SetMesh(m_mesh);
        }
    }

    double Terrain::GetHeightAt(double worldX, double worldZ) const noexcept
    {
        return m_generator.GenerateHeight(worldX, worldZ);
    }
}

