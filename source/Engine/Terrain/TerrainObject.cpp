// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "TerrainObject.h"
#include "../LightChannel.h"
#include "Renderer/Material.h"

namespace Sandbox3D::Engine::Terrain
{
    TerrainObject::TerrainObject(const TerrainConfig& config, std::string_view name)
        : Body(name)
        , m_config(config)
        , m_generator(config)
        , m_width(config.width)
        , m_depth(config.depth)
    {
        SetMaterial(std::make_shared<TerrainMaterial>());
        SetLightChannels(LightChannel::Terrain);
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
        m_width = config.width;
        m_depth = config.depth;
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

    void TerrainObject::SetElevationGrid(
        uint32_t resX,
        uint32_t resZ,
        double width,
        double depth,
        std::vector<float> elevations,
        double centerElevation,
        double scaleXZ,
        double scaleY,
        double minElevation,
        double maxElevation
    )
    {
        m_resX = resX;
        m_resZ = resZ;
        m_width = width;
        m_depth = depth;
        m_config.width = width;
        m_config.depth = depth;
        m_elevations = std::move(elevations);
        m_centerElevation = centerElevation;
        m_scaleXZ = scaleXZ;
        m_scaleY = scaleY;

        auto collider = std::make_shared<TerrainCollider>(
            m_resX,
            m_resZ,
            m_width,
            m_depth,
            m_elevations,
            m_centerElevation,
            m_scaleXZ,
            m_scaleY,
            minElevation,
            maxElevation
        );
        SetCollider(std::move(collider));
    }

    bool TerrainObject::InitialiseClipmap(
        ID3D12Device* device,
        uint32_t patchQuads,
        uint32_t maxLodLevels
    )
    {
        if (!device)
        {
            return false;
        }

        m_clipmap = std::make_shared<TerrainClipmap>();
        return m_clipmap->Initialise(
            device,
            patchQuads,
            maxLodLevels,
            -m_width * 0.5,
            m_width * 0.5,
            -m_depth * 0.5,
            m_depth * 0.5,
            m_centerElevation,
            m_elevations,
            m_resX,
            m_resZ
        );
    }

    void TerrainObject::UpdateClipmap(
        const Maths::Vec3D& cameraPosition,
        const Maths::BoundingFrustumD& frustum
    ) noexcept
    {
        if (m_useClipmap && m_clipmap && m_clipmap->IsInitialised())
        {
            m_clipmap->Update(cameraPosition, frustum);
        }
    }

    void TerrainObject::GenerateClipmapRenderItems(std::vector<Renderer::RenderItem>& outItems) const
    {
        if (m_useClipmap && m_clipmap && m_clipmap->IsInitialised())
        {
            m_clipmap->GenerateRenderItems(GetMaterial(), outItems);
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

    Maths::Vec3D TerrainObject::GetNormalAt(double worldX, double worldZ) const noexcept
    {
        if (auto terrainCol = GetTerrainCollider())
        {
            return terrainCol->GetNormalAt(worldX, worldZ);
        }
        return Maths::Vec3D::Up();
    }

    std::shared_ptr<TerrainCollider> TerrainObject::GetTerrainCollider() const noexcept
    {
        return std::dynamic_pointer_cast<TerrainCollider>(m_collider);
    }

    std::shared_ptr<TerrainMaterial> TerrainObject::GetTerrainMaterial() const noexcept
    {
        return std::dynamic_pointer_cast<TerrainMaterial>(GetMaterial());
    }

    void TerrainObject::SetTerrainMaterial(std::shared_ptr<TerrainMaterial> material)
    {
        SetMaterial(std::move(material));
    }

    void TerrainObject::SetLidarOcclusionMaps(
        ID3D12Device* device,
        ID3D12CommandQueue* commandQueue,
        const void* aoPixelData,
        const void* horizon0PixelData,
        const void* horizon1PixelData,
        uint32_t width,
        uint32_t height
    )
    {
        if (auto mat = GetTerrainMaterial())
        {
            mat->SetLidarOcclusionMaps(device, commandQueue, aoPixelData, horizon0PixelData, horizon1PixelData, width, height);
        }
    }

    void TerrainObject::SetTerrainGpuMaps(
        ID3D12Device* device,
        ID3D12CommandQueue* commandQueue,
        const float* heightfieldData,
        const int16_t* macroNormalData,
        const uint16_t* paletteColorData,
        uint32_t width,
        uint32_t height
    )
    {
        if (auto mat = GetTerrainMaterial())
        {
            mat->SetTerrainGpuMaps(device, commandQueue, heightfieldData, macroNormalData, paletteColorData, width, height);
        }
    }
}
