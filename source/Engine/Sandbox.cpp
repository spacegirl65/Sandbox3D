// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Sandbox.h"
#include "Terrain/TerrainMesh.h"
#include "Terrain/TerrainCollider.h"
#include "Terrain/TerrainMaterial.h"
#include "Core/Time.h"
#include "Maths/Maths.h"

#include <d3d12.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <windows.h>

namespace Sandbox3D
{
    using namespace Maths;
    using Engine::LightChannel;

    Sandbox::Sandbox(Renderer::Renderer& renderer, ID3D12Device* device, ID3D12CommandQueue* commandQueue)
        : m_renderer(renderer)
    {
        // Initialise and register active scene camera as a Base object, binding non-owning pointer to Renderer
        m_camera = CreateObject<Engine::Camera>();
        m_renderer.SetCamera(m_camera.get());

        // Configure summer sky clear colour, atmospheric aerial perspective, and warm balanced ambient fill
        m_renderer.SetClearColor(Maths::Vec4(0.718f, 0.865f, 0.986f, 1.0f));
        m_renderer.SetFogColour(Maths::Vec4(0.718f, 0.865f, 0.986f, 0.15f));
        m_renderer.SetFogParams(3500.0f, 12000.0f, 0.00008f);
        m_renderer.SetAmbientColor(Maths::Vec4(0.22f, 0.22f, 0.20f, 1.0f));
        m_renderer.SetAtmosphereParameters(
            m_atmosphereParams.rayleighParams,
            m_atmosphereParams.mieParams,
            m_atmosphereParams.ozoneParams,
            m_atmosphereParams.planetParams
        );

        // Primary directional sun affecting all channels (terrain and character)
        m_sunLight = CreateLight("JulySummerSun");
        m_sunLight->SetChannels(LightChannel::All);

        // Secondary directional ground bounce and valley point light applying exclusively to terrain
        m_earthBounceLight = CreateLight("SummerGroundBounce");
        m_earthBounceLight->SetColourTemperature(4200.0f);
        m_earthBounceLight->SetChannels(LightChannel::Terrain);

        // Synchronise initial celestial sun vector and ground bounce with ephemeris
        UpdateSunDirection();

        auto summerPoint = CreateLight(
            Maths::Vec3D(0.0, 75.0, 0.0),
            350.0f,
            Maths::Vec4(1.0f, 1.0f, 1.0f, 1.0f),
            "SummerValleyPointLight"
        );
        summerPoint->SetColourTemperature(4800.0f);
        summerPoint->SetChannels(LightChannel::Terrain);

        // Configure and load Garsdale LiDAR northern half terrain mesh (.mesh, 15000m x 7500m)
        Engine::Terrain::TerrainConfig terrainConfig{};
        terrainConfig.width  = 15000.0;
        terrainConfig.depth  = 7500.0;
        terrainConfig.origin = Maths::Vec3D(0.0, 0.0, 0.0);

        Renderer::MeshFileHeader terrainMeshHeader{};
        Engine::Terrain::TerrainMeshData lidarMeshData = Engine::Terrain::TerrainMesh::GenerateFromFile(
            "resources/environment/terrain/garsdale.mesh",
            Maths::Vec3D(0.0, 0.0, 0.0),
            &terrainMeshHeader
        );

        // Instantiate primary terrain body registered in the scene graph
        m_terrain = CreateBody<Engine::Terrain::TerrainObject>(terrainConfig);

        if (!lidarMeshData.IsEmpty() && device)
        {
            constexpr uint32_t standardGridResX = 1000u;
            const uint32_t resX = (terrainMeshHeader.vertexCount == 1000000u || terrainMeshHeader.vertexCount == 500000u)
                ? standardGridResX
                : (terrainMeshHeader.vertexCount > 0)
                    ? static_cast<uint32_t>(std::round(std::sqrt(static_cast<double>(terrainMeshHeader.vertexCount))))
                    : standardGridResX;
            const uint32_t resZ = (resX > 0) ? static_cast<uint32_t>(lidarMeshData.vertices.size() / resX) : 500u;

            const double meshExtentX = (terrainMeshHeader.maxX > terrainMeshHeader.minX)
                ? static_cast<double>(terrainMeshHeader.maxX - terrainMeshHeader.minX)
                : ((terrainMeshHeader.width > 0.0) ? terrainMeshHeader.width : 15000.0);
            const double meshExtentZ = (terrainMeshHeader.maxZ > terrainMeshHeader.minZ)
                ? static_cast<double>(terrainMeshHeader.maxZ - terrainMeshHeader.minZ)
                : ((terrainMeshHeader.depth > 0.0) ? terrainMeshHeader.depth : 7500.0);

            terrainConfig.width  = meshExtentX;
            terrainConfig.depth  = meshExtentZ;

            Engine::Terrain::TerrainMesh::ApplyProceduralPalette(
                lidarMeshData.vertices,
                terrainConfig,
                terrainMeshHeader.minElevation,
                terrainMeshHeader.maxElevation,
                resX,
                resZ
            );

            auto terrainMesh = std::make_shared<Renderer::Mesh>();
            std::vector<Renderer::MeshCluster> terrainClusters;
            if (resX >= 2 && resZ >= 2 && lidarMeshData.vertices.size() == static_cast<size_t>(resX) * resZ)
            {
                lidarMeshData.indices = Engine::Terrain::TerrainMesh::GenerateTiledAlternatingIndices(
                    lidarMeshData.vertices,
                    resX,
                    resZ,
                    terrainClusters
                );
            }
            terrainMesh->Initialise(device, lidarMeshData.vertices, lidarMeshData.indices);
            terrainMesh->SetClusters(std::move(terrainClusters));

            constexpr double centerElevation = 333.794;
            constexpr double scaleXZ = 1.0;
            constexpr double scaleY  = 1.0;

            // Cache elevation grid for continuous terrain elevation sampling
            std::vector<float> terrainElevations(lidarMeshData.vertices.size());
            for (size_t i = 0; i < lidarMeshData.vertices.size(); ++i)
            {
                terrainElevations[i] = lidarMeshData.vertices[i].position.y;
            }

            // Configure Yorkshire Dales terrain surface material biomes and stage PBR textures into GPU descriptor table
            if (device && commandQueue && m_terrain)
            {
                auto terrainMaterial = std::make_shared<Engine::Terrain::TerrainMaterial>("resources/environment/terrain/textures");

                // Clover and grazed pasture meadow
                terrainMaterial->AddPrimaryLayer(Engine::Terrain::TerrainLayer{
                    .name             = "Meadow",
                    .assetId          = "1",
                    .albedoHeightFile = "uncut_grass_1_albedo_height.dds",
                    .normalFile       = "uncut_grass_1_normal.dds",
                    .ormFile          = "uncut_grass_1_orm.dds",
                    .uvScale          = 0.50f
                });

                // Mid-slope fell turf
                terrainMaterial->AddPrimaryLayer(Engine::Terrain::TerrainLayer{
                    .name             = "MidSlope",
                    .assetId          = "1",
                    .albedoHeightFile = "wild_grass_1_albedo_height.dds",
                    .normalFile       = "wild_grass_1_normal.dds",
                    .ormFile          = "wild_grass_1_orm.dds",
                    .uvScale          = 0.50f
                });

                // High plateau moorland
                terrainMaterial->AddPrimaryLayer(Engine::Terrain::TerrainLayer{
                    .name             = "Plateau",
                    .assetId          = "2",
                    .albedoHeightFile = "wild_grass_2_albedo_height.dds",
                    .normalFile       = "wild_grass_2_normal.dds",
                    .ormFile          = "wild_grass_2_orm.dds",
                    .uvScale          = 0.50f
                });

                // Dry thatch knolls and weathered ridges
                terrainMaterial->AddPrimaryLayer(Engine::Terrain::TerrainLayer{
                    .name             = "DryThatch",
                    .assetId          = "1",
                    .albedoHeightFile = "grass_dried_1_albedo_height.dds",
                    .normalFile       = "grass_dried_1_normal.dds",
                    .ormFile          = "grass_dried_1_orm.dds",
                    .uvScale          = 0.50f
                });

                // Crevice, scar, and sheer cliff rock
                terrainMaterial->AddPrimaryLayer(Engine::Terrain::TerrainLayer{
                    .name             = "RockCliff",
                    .assetId          = "1",
                    .albedoHeightFile = "rock_cliff_1_albedo_height.dds",
                    .normalFile       = "rock_cliff_1_normal.dds",
                    .ormFile          = "rock_cliff_1_orm.dds",
                    .uvScale          = 0.50f
                });

                // Alluvial valley pasture and meadow
                terrainMaterial->AddAlternativeLayer(Engine::Terrain::TerrainLayer{
                    .name             = "AltMeadow",
                    .assetId          = "2",
                    .albedoHeightFile = "uncut_grass_2_albedo_height.dds",
                    .normalFile       = "uncut_grass_2_normal.dds",
                    .ormFile          = "uncut_grass_2_orm.dds",
                    .uvScale          = 0.50f
                });

                // Fine moor-edge bent-grass
                terrainMaterial->AddAlternativeLayer(Engine::Terrain::TerrainLayer{
                    .name             = "AltMidSlope",
                    .assetId          = "3",
                    .albedoHeightFile = "wild_grass_3_albedo_height.dds",
                    .normalFile       = "wild_grass_3_normal.dds",
                    .ormFile          = "wild_grass_3_orm.dds",
                    .uvScale          = 0.50f
                });

                // Weathered crag alpine turf
                terrainMaterial->AddAlternativeLayer(Engine::Terrain::TerrainLayer{
                    .name             = "AltPlateau",
                    .assetId          = "4",
                    .albedoHeightFile = "wild_grass_4_albedo_height.dds",
                    .normalFile       = "wild_grass_4_normal.dds",
                    .ormFile          = "wild_grass_4_orm.dds",
                    .uvScale          = 0.50f
                });

                terrainMaterial->Load(device, commandQueue);
                terrainMaterial->SetShadowMap(device, m_renderer.GetCascadedShadowMap());
                m_terrain->SetTerrainMaterial(std::move(terrainMaterial));
            }

            // Pre-bake LiDAR horizon ambient occlusion, multi-scale crevice depth, and directional horizon angle maps
            const auto lidarOcclusionMaps = Engine::Terrain::TerrainMesh::ComputeLidarHorizonOcclusion(
                terrainElevations,
                resX,
                resZ,
                meshExtentX,
                meshExtentZ
            );
            if (!lidarOcclusionMaps.IsEmpty() && device && commandQueue && m_terrain)
            {
                m_terrain->SetLidarOcclusionMaps(
                    device,
                    commandQueue,
                    lidarOcclusionMaps.ambientOcclusionMap.data(),
                    lidarOcclusionMaps.horizonAnglesMap0.data(),
                    lidarOcclusionMaps.horizonAnglesMap1.data(),
                    resX,
                    resZ
                );
            }

            // Build and upload GPU heightfield, continuous macro normal, and landscape palette textures
            const auto terrainGpuMaps = Engine::Terrain::TerrainMesh::ComputeTerrainGpuMaps(
                terrainElevations,
                lidarMeshData.vertices,
                resX,
                resZ,
                meshExtentX,
                meshExtentZ
            );
            if (!terrainGpuMaps.IsEmpty() && device && commandQueue && m_terrain)
            {
                m_terrain->SetTerrainGpuMaps(
                    device,
                    commandQueue,
                    terrainGpuMaps.heightfieldMap.data(),
                    terrainGpuMaps.macroNormalMap.data(),
                    terrainGpuMaps.paletteColorMap.data(),
                    resX,
                    resZ
                );
            }

            if (m_terrain)
            {
                m_terrain->SetMesh(std::move(terrainMesh));
                m_terrain->SetWorldMatrix(Maths::Mat4x4D::Translation(0.0, -centerElevation, 0.0));
                m_terrain->SetElevationGrid(
                    resX,
                    resZ,
                    meshExtentX,
                    meshExtentZ,
                    std::move(terrainElevations),
                    centerElevation,
                    scaleXZ,
                    scaleY
                );
                m_terrain->InitialiseClipmap(device, 64, 5);
            }

            // Post scene initialisation
            m_renderer.OnSceneInitialised();

            std::wcout << L"[Sandbox] Loaded terrain successfully from garsdale.mesh:\n";
            std::wcout << L"          Sub-mesh Bounds: [" << terrainMeshHeader.minX << L", " << terrainMeshHeader.minY << L", " << terrainMeshHeader.minZ << L"] to ["
                       << terrainMeshHeader.maxX << L", " << terrainMeshHeader.maxY << L", " << terrainMeshHeader.maxZ << L"]\n";
            std::wcout << L"          Elevation Range: " << terrainMeshHeader.minElevation << L"m - " << terrainMeshHeader.maxElevation << L"m\n";
            std::wcout << L"          Status: Full 1:1 Scale (15,000m length x 7,500m width) and centered at origin.\n";
        }
        else
        {
            std::wcout << L"[Sandbox] Notice: garsdale.mesh could not be loaded.\n";
        }

        // Initialise debug spatial cell wireframe meshes and material
        if (device)
        {
            m_debugCellMesh = Renderer::Mesh::CreateWireframeBox(
                device,
                1.0f,
                0.002f,
                Maths::Vec4::White(),
                Maths::Vec4(1.0f, 1.0f, 1.0f, 0.0f)
            );
            m_activeCellMesh = Renderer::Mesh::CreateWireframeBox(
                device,
                1.0f,
                0.002f,
                Maths::Vec4::White(),
                Maths::Vec4(1.0f, 1.0f, 1.0f, 0.15f)
            );
        }
        m_debugCellMaterial = Renderer::Material::CreateUnlit(Maths::Vec4::White(), "DebugCellMaterial");

        // Load pre-compiled character mesh directly via Renderer::Mesh (no render device passed to Body/Base subclasses)
        constexpr std::string_view characterMeshPath = "resources/entities/character.mesh";
        std::shared_ptr<Renderer::Mesh> characterMesh;
        if (device)
        {
            characterMesh = Renderer::Mesh::LoadFromFile(device, characterMeshPath);
        }

        // Instantiate player character entity possessing loaded mesh and internal eye camera
        m_character = CreateBody<Engine::Character>(characterMesh, "PlayerCharacter");
        constexpr double groundClearance = 0.02; // Initial clearance of 2 cm above ground turf
        constexpr double playerSpawnX    = 0.0;
        constexpr double playerSpawnZ    = -2650.0; // Crest of Rise Hill overlooking Garsdale
        const double groundHeight = m_terrain ? m_terrain->GetHeightAt(playerSpawnX, playerSpawnZ) : 0.0;
        const double spawnY = groundHeight + groundClearance;
        m_character->SetPosition(Maths::Vec3D(playerSpawnX, spawnY, playerSpawnZ));

        // Orient player camera on Rise Hill facing north-northeast down the length of the dale
        constexpr double playerInitialYaw   = 0.45;  // ~26 degrees East of North
        constexpr double playerInitialPitch = -0.05; // Gently angled downward across the dale floor
        m_character->SetOrientation(playerInitialYaw, playerInitialPitch);

        std::wcout << L"[Sandbox] Spawned player character at: ("
                   << playerSpawnX << L", " << spawnY << L", " << playerSpawnZ << L") [altitude: " << groundHeight <<"m]\n";

        // Initialise spatial grid partitioning around primary terrain bounds
        if (m_terrain)
        {
            RebuildSpatialGrid(m_terrain->GetConfig(), -1000.0, 1000.0);
        }

        // Ensure initial camera position, orientation vectors, and active camera pointer are fully synchronised
        SetCameraPosition(Maths::Vec3D(-1696.5, 1230.1, -2020.1), Maths::DegToRad<double> * 58.0, -Maths::DegToRad<double> * 21.0);
        if (auto* activeCamera = GetActiveCamera())
        {
            m_renderer.SetCamera(activeCamera);
        }

        // Hide orientation gizmo if starting in player camera mode
        m_renderer.SetShowGizmo(m_useSpectatorCamera);
    }

    void Sandbox::AddObject(std::shared_ptr<Engine::Base> object)
    {
        if (object)
        {
            m_objects.push_back(object);
            if (auto body = std::dynamic_pointer_cast<Engine::Body>(object))
            {
                if (std::find(m_bodies.begin(), m_bodies.end(), body) == m_bodies.end())
                {
                    m_bodies.push_back(std::move(body));
                }
            }
            if (auto terrain = std::dynamic_pointer_cast<Engine::Terrain::TerrainObject>(object))
            {
                if (std::find(m_terrains.begin(), m_terrains.end(), terrain) == m_terrains.end())
                {
                    m_terrains.push_back(terrain);
                }
            }
            if (auto light = std::dynamic_pointer_cast<Engine::Light>(object))
            {
                if (std::find(m_lights.begin(), m_lights.end(), light) == m_lights.end())
                {
                    m_lights.push_back(std::move(light));
                }
            }
        }
    }

    void Sandbox::AddBody(std::shared_ptr<Engine::Body> body)
    {
        if (body)
        {
            AddObject(body);
            if (!body->IsTerrainCollider())
            {
                m_spatialGrid.InsertEntity(body, body->GetWorldBoundingBox().GetCenter());
            }
        }
    }

    void Sandbox::AddTerrain(std::shared_ptr<Engine::Terrain::TerrainObject> terrain)
    {
        if (terrain)
        {
            AddObject(terrain);
        }
    }

    void Sandbox::AddLight(std::shared_ptr<Engine::Light> light)
    {
        if (light)
        {
            AddObject(light);
        }
    }

    void Sandbox::RemoveObject(std::string_view name)
    {
        if (const auto obj = FindObject(name))
        {
            m_spatialGrid.RemoveEntity(obj->GetId());
        }
        std::erase_if(m_objects, [name](const std::shared_ptr<Engine::Base>& obj) {
            return obj && obj->GetName() == name;
        });
        std::erase_if(m_bodies, [name](const std::shared_ptr<Engine::Body>& body) {
            return body && body->GetName() == name;
        });
        std::erase_if(m_terrains, [name](const std::shared_ptr<Engine::Terrain::TerrainObject>& terrain) {
            return terrain && terrain->GetName() == name;
        });
        std::erase_if(m_lights, [name](const std::shared_ptr<Engine::Light>& light) {
            return light && light->GetName() == name;
        });
        if (m_terrain && m_terrain->GetName() == name)
        {
            m_terrain.reset();
        }
        if (m_sunLight && m_sunLight->GetName() == name)
        {
            m_sunLight.reset();
        }
        if (m_earthBounceLight && m_earthBounceLight->GetName() == name)
        {
            m_earthBounceLight.reset();
        }
    }

    void Sandbox::RemoveObject(uint32_t id)
    {
        m_spatialGrid.RemoveEntity(id);
        std::erase_if(m_objects, [id](const std::shared_ptr<Engine::Base>& obj) {
            return obj && obj->GetId() == id;
        });
        std::erase_if(m_bodies, [id](const std::shared_ptr<Engine::Body>& body) {
            return body && body->GetId() == id;
        });
        std::erase_if(m_terrains, [id](const std::shared_ptr<Engine::Terrain::TerrainObject>& terrain) {
            return terrain && terrain->GetId() == id;
        });
        std::erase_if(m_lights, [id](const std::shared_ptr<Engine::Light>& light) {
            return light && light->GetId() == id;
        });
        if (m_terrain && m_terrain->GetId() == id)
        {
            m_terrain.reset();
        }
        if (m_sunLight && m_sunLight->GetId() == id)
        {
            m_sunLight.reset();
        }
        if (m_earthBounceLight && m_earthBounceLight->GetId() == id)
        {
            m_earthBounceLight.reset();
        }
    }

    void Sandbox::RemoveBody(std::string_view name)
    {
        RemoveObject(name);
    }

    void Sandbox::RemoveBody(uint32_t id)
    {
        RemoveObject(id);
    }

    void Sandbox::RemoveTerrain(std::string_view name)
    {
        RemoveObject(name);
    }

    void Sandbox::RemoveTerrain(uint32_t id)
    {
        RemoveObject(id);
    }

    void Sandbox::RemoveLight(std::string_view name)
    {
        RemoveObject(name);
    }

    void Sandbox::RemoveLight(uint32_t id)
    {
        RemoveObject(id);
    }

    std::shared_ptr<Engine::Base> Sandbox::FindObject(std::string_view name) const noexcept
    {
        for (const auto& obj : m_objects)
        {
            if (obj && obj->GetName() == name)
            {
                return obj;
            }
        }
        return nullptr;
    }

    std::shared_ptr<Engine::Body> Sandbox::FindBody(std::string_view name) const noexcept
    {
        for (const auto& body : m_bodies)
        {
            if (body && body->GetName() == name)
            {
                return body;
            }
        }
        return nullptr;
    }

    std::shared_ptr<Engine::Terrain::TerrainObject> Sandbox::FindTerrain(std::string_view name) const noexcept
    {
        for (const auto& terrain : m_terrains)
        {
            if (terrain && terrain->GetName() == name)
            {
                return terrain;
            }
        }
        return nullptr;
    }

    std::shared_ptr<Engine::Light> Sandbox::FindLight(std::string_view name) const noexcept
    {
        for (const auto& light : m_lights)
        {
            if (light && light->GetName() == name)
            {
                return light;
            }
        }
        return nullptr;
    }
    
    void Sandbox::AddRenderItem(Renderer::RenderItem item)
    {
        m_renderItems.push_back(std::move(item));
    }

    void Sandbox::AddRenderItem(std::shared_ptr<Renderer::Mesh> mesh, const Maths::Mat4x4D& worldMatrix, const std::string& name)
    {
        m_renderItems.push_back(Renderer::RenderItem{ std::move(mesh), worldMatrix, true, name });
    }

    void Sandbox::AddRenderItem(std::shared_ptr<Renderer::Mesh> mesh, std::shared_ptr<Renderer::Material> material, const Maths::Mat4x4D& worldMatrix, const std::string& name)
    {
        m_renderItems.push_back(Renderer::RenderItem{ std::move(mesh), std::move(material), worldMatrix, true, name });
    }

    void Sandbox::RemoveRenderItem(std::string_view name)
    {
        std::erase_if(m_renderItems, [name](const Renderer::RenderItem& item) { return item.name == name; });
    }

    void Sandbox::ClearRenderItems() noexcept
    {
        m_renderItems.clear();
    }

    std::span<const Renderer::RenderItem> Sandbox::GetRenderItems() const noexcept
    {
        m_cachedRenderItems.clear();

        // Collect render items from all renderable, visible, active Base objects
        for (const auto& obj : m_objects)
        {
            if (obj && obj->IsActive() && obj->IsRenderable() && obj->IsVisible())
            {
                // When a terrain object has an active clipmap, replace the monolithic terrain mesh with nested clipmap patches
                if (const auto terrain = std::dynamic_pointer_cast<Engine::Terrain::TerrainObject>(obj))
                {
                    if (terrain->IsUsingClipmap())
                    {
                        terrain->GenerateClipmapRenderItems(m_cachedRenderItems);
                        continue;
                    }
                }

                for (const auto& item : obj->GetRenderItems())
                {
                    if (item.mesh && item.isVisible)
                    {
                        m_cachedRenderItems.push_back(item);
                    }
                }
            }
        }

        // Append any standalone render items added directly via AddRenderItem
        for (const auto& item : m_renderItems)
        {
            if (item.mesh && item.isVisible)
            {
                m_cachedRenderItems.push_back(item);
            }
        }

        // Append spatial cell debug wireframe boxes when toggled visible
        if (m_showDebugCells && m_debugCellMesh)
        {
            const auto* activeCamera = GetActiveCamera();
            const Maths::Vec3D cameraPos = activeCamera ? activeCamera->GetPosition() : Maths::Vec3D::Zero();
            const Engine::CellCoord activeCoord = m_spatialGrid.GetCellCoord(cameraPos);
            const auto activeCell = m_spatialGrid.FindCell(activeCoord);
            const bool isCameraInsideGridCell = (activeCell != nullptr && activeCell->IsOccupied());

            for (const auto* cell : m_visibleCells)
            {
                if (cell && cell->IsVisible() && cell->IsOccupied())
                {
                    // Defer camera-occupied cell to render with m_activeCellMesh (shaded top/bottom faces) below
                    if (isCameraInsideGridCell && cell->GetCoord() == activeCoord)
                    {
                        continue;
                    }
                    m_cachedRenderItems.push_back(cell->CreateDebugRenderItem(m_debugCellMesh, m_debugCellMaterial));
                }
            }

            // When a camera (spectator or otherwise) is inside an occupied grid cell, render it with top and bottom shaded faces
            if (isCameraInsideGridCell && m_activeCellMesh)
            {
                m_cachedRenderItems.push_back(activeCell->CreateDebugRenderItem(m_activeCellMesh, m_debugCellMaterial));
            }
        }

        return m_cachedRenderItems;
    }

    std::span<const Renderer::GpuLight> Sandbox::GetLightData() const noexcept
    {
        m_cachedGpuLights.clear();
        const auto* activeCamera = GetActiveCamera();
        const Maths::Vec3D cameraPos = activeCamera ? activeCamera->GetPosition() : Maths::Vec3D::Zero();

        for (const auto& light : m_lights)
        {
            if (light && light->IsActive())
            {
                if (m_cachedGpuLights.size() < Renderer::MaxLights)
                {
                    m_cachedGpuLights.push_back(light->ToGpuLight(cameraPos));
                }
            }
        }

        return m_cachedGpuLights;
    }

    Renderer::RenderItem* Sandbox::FindRenderItem(std::string_view name) noexcept
    {
        for (auto& item : m_renderItems)
        {
            if (item.name == name)
            {
                return &item;
            }
        }
        return nullptr;
    }

    const Renderer::RenderItem* Sandbox::FindRenderItem(std::string_view name) const noexcept
    {
        for (const auto& item : m_renderItems)
        {
            if (item.name == name)
            {
                return &item;
            }
        }
        return nullptr;
    }

    void Sandbox::RebuildSpatialGrid(const Engine::Terrain::TerrainConfig& config, [[maybe_unused]] double minY, [[maybe_unused]] double maxY)
    {
        constexpr double baseCellSize = 1000.0;
        m_spatialGrid.Clear();
        m_spatialGrid.SetBaseCellSize(baseCellSize);

        const double minX = config.origin.x - config.width * 0.5;
        const double maxX = config.origin.x + config.width * 0.5;
        const double minZ = config.origin.z - config.depth * 0.5;
        const double maxZ = config.origin.z + config.depth * 0.5;

        const int64_t startX = static_cast<int64_t>(std::floor(minX / baseCellSize));
        const int64_t endX   = static_cast<int64_t>(std::ceil(maxX / baseCellSize));
        const int64_t startZ = static_cast<int64_t>(std::floor(minZ / baseCellSize));
        const int64_t endZ   = static_cast<int64_t>(std::ceil(maxZ / baseCellSize));

        for (int64_t iz = startZ; iz < endZ; ++iz)
        {
            const double cellMinZ = static_cast<double>(iz) * baseCellSize;
            const double cellMaxZ = cellMinZ + baseCellSize;
            const double cellMidZ = cellMinZ + baseCellSize * 0.5;

            for (int64_t ix = startX; ix < endX; ++ix)
            {
                const double cellMinX = static_cast<double>(ix) * baseCellSize;
                const double cellMaxX = cellMinX + baseCellSize;
                const double cellMidX = cellMinX + baseCellSize * 0.5;

                // Sample terrain elevation across this cell column to identify occupied vertical range
                double colMinY = 0.0;
                double colMaxY = 0.0;

                if (m_terrain)
                {
                    const double h0 = m_terrain->GetHeightAt(cellMinX, cellMinZ);
                    const double h1 = m_terrain->GetHeightAt(cellMaxX, cellMinZ);
                    const double h2 = m_terrain->GetHeightAt(cellMinX, cellMaxZ);
                    const double h3 = m_terrain->GetHeightAt(cellMaxX, cellMaxZ);
                    const double h4 = m_terrain->GetHeightAt(cellMidX, cellMidZ);

                    colMinY = std::min({ h0, h1, h2, h3, h4 });
                    colMaxY = std::max({ h0, h1, h2, h3, h4 });
                }

                const int64_t startY = static_cast<int64_t>(std::floor(colMinY / baseCellSize));
                const int64_t endY   = static_cast<int64_t>(std::floor(colMaxY / baseCellSize));

                for (int64_t iy = startY; iy <= endY; ++iy)
                {
                    auto cell = m_spatialGrid.GetOrCreateCell(Engine::CellCoord(ix, iy, iz, 0));
                    cell->SetHasTerrain(true);
                }
            }
        }

        // Re-index all active non-terrain physical bodies into the spatial grid
        for (const auto& body : m_bodies)
        {
            if (body && !body->IsTerrainCollider())
            {
                m_spatialGrid.InsertEntity(body, body->GetWorldBoundingBox().GetCenter());
            }
        }
    }

    Engine::Camera* Sandbox::GetActiveCamera() noexcept
    {
        if (!m_useSpectatorCamera && m_character && m_character->GetEyeCamera())
        {
            return m_character->GetEyeCamera();
        }
        return m_camera.get();
    }

    const Engine::Camera* Sandbox::GetActiveCamera() const noexcept
    {
        if (!m_useSpectatorCamera && m_character && m_character->GetEyeCamera())
        {
            return m_character->GetEyeCamera();
        }
        return m_camera.get();
    }

    void Sandbox::ToggleCameraMode() noexcept
    {
        m_useSpectatorCamera = !m_useSpectatorCamera;
        m_hasLastMousePos = false;
        m_renderer.SetShowGizmo(m_useSpectatorCamera);
        auto* activeCamera = GetActiveCamera();
        if (activeCamera)
        {
            m_renderer.SetCamera(activeCamera);
            m_visibleCells.clear();
            m_spatialGrid.UpdateVisibility(*activeCamera, m_visibleCells);
        }
        if (m_character)
        {
            m_character->SetHorizontalSpeed(Maths::Vec3D{ 0.0, 0.0, 0.0 });
        }
        std::wcout << L"[Sandbox] Switched camera mode: "
                   << (m_useSpectatorCamera ? L"Spectator View" : L"Character View");
    }

    void Sandbox::SetSpectatorCameraActive(bool active) noexcept
    {
        if (m_useSpectatorCamera != active)
        {
            ToggleCameraMode();
        }
    }

    void Sandbox::SetCameraPosition(const Maths::Vec3D& position)
    {
        m_initialCameraPosition = position;
        m_cameraPosition        = position;

        // Target coordinate origin if spectator camera is active, otherwise character position
        const Vec3D initialTarget = (!m_useSpectatorCamera && m_character)
            ? Maths::Vec3D(m_character->GetPosition().x, m_character->GetPosition().y + 0.885, m_character->GetPosition().z)
            : Maths::Vec3D(0.0, 0.0, 0.0);
        const Vec3D toTarget = initialTarget - position;
        const double horizontalDist = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);

        m_cameraPitch = std::atan2(toTarget.y, horizontalDist);
        m_cameraYaw   = std::atan2(toTarget.x, toTarget.z);

        UpdateCameraVectors();
    }

    void Sandbox::SetCameraPosition(const Maths::Vec3D& position, double yawRadians, double pitchRadians)
    {
        m_initialCameraPosition = position;
        m_cameraPosition        = position;
        m_cameraYaw             = yawRadians;
        m_cameraPitch           = pitchRadians;

        UpdateCameraVectors();
    }

    void Sandbox::UpdateCameraVectors()
    {
        // Forward and orthogonal up vectors derived directly from orientation quaternion
        const QuatD cameraQuat = QuatD::FromEulerAngles(m_cameraPitch, m_cameraYaw, 0.0);
        const Vec3D forward    = cameraQuat.Rotate(Vec3D::Forward());
        const Vec3D cameraUp   = cameraQuat.Rotate(Vec3D::Up());

        m_cameraTarget = m_cameraPosition + forward;

        if (m_camera)
        {
            m_camera->SetLookAt(m_cameraPosition, m_cameraTarget, cameraUp);
        }
    }

    void Sandbox::SetSunAzimuth(float azimuthRadians) noexcept
    {
        m_sunAzimuth = std::fmod(azimuthRadians, Maths::TwoPi<float>);
        if (m_sunAzimuth < 0.0f)
        {
            m_sunAzimuth += Maths::TwoPi<float>;
        }
        UpdateSunDirection();
    }

    void Sandbox::SetSunElevation(float elevationRadians) noexcept
    {
        constexpr float minElev = -Maths::DegToRad<float> * 15.0f;
        constexpr float maxElev =  Maths::DegToRad<float> * 89.9f;
        m_sunElevation = std::clamp(elevationRadians, minElev, maxElev);
        UpdateSunDirection();
    }

    void Sandbox::SetSunAngles(float azimuthRadians, float elevationRadians) noexcept
    {
        m_sunAzimuth = std::fmod(azimuthRadians, Maths::TwoPi<float>);
        if (m_sunAzimuth < 0.0f)
        {
            m_sunAzimuth += Maths::TwoPi<float>;
        }

        constexpr float minElev = -Maths::DegToRad<float> * 15.0f;
        constexpr float maxElev =  Maths::DegToRad<float> * 89.9f;
        m_sunElevation = std::clamp(elevationRadians, minElev, maxElev);
        UpdateSunDirection();
    }

    void Sandbox::UpdateSunDirection() noexcept
    {
        const Maths::Vec3 sunVector = Engine::AtmosphereParameters::DirectionFromAzimuthElevation(
            m_sunAzimuth,
            m_sunElevation
        );

        // Light rays travel downward from the celestial sun towards the ground
        const Maths::Vec3 lightDir = -sunVector;

        if (m_sunLight)
        {
            m_sunLight->SetDirection(lightDir);

            // Modulate solar illuminance and colour temperature across the diurnal cycle
            const float sinElev = std::sin(m_sunElevation);
            if (sinElev > 0.0f)
            {
                // Day to golden hour transition: attenuate smoothly as the sun grazes the horizon
                const float intensityFactor = std::clamp(sinElev * 1.5f, 0.0f, 1.0f);
                m_sunLight->SetIntensity(1.05f * intensityFactor);

                // Golden hour reddening near horizon (~2800K) transitioning to high midday (~5000K)
                const float kelvin = std::lerp(2800.0f, 5000.0f, std::clamp(sinElev * 2.0f, 0.0f, 1.0f));
                m_sunLight->SetColourTemperature(kelvin);
            }
            else
            {
                // Sun below the horizon (night phase)
                m_sunLight->SetIntensity(0.0f);
            }
        }

        if (m_earthBounceLight)
        {
            // Upward ground bounce reflects opposite to downward incident sunlight
            m_earthBounceLight->SetDirection(sunVector);
            const float sinElev = std::sin(m_sunElevation);
            const float bounceFactor = std::clamp(sinElev * 1.5f, 0.0f, 1.0f);
            m_earthBounceLight->SetIntensity(0.15f * bounceFactor);
        }
    }

    void Sandbox::SetAtmosphereParameters(const Engine::AtmosphereParameters& params) noexcept
    {
        m_atmosphereParams = params;
        m_renderer.SetAtmosphereParameters(
            m_atmosphereParams.rayleighParams,
            m_atmosphereParams.mieParams,
            m_atmosphereParams.ozoneParams,
            m_atmosphereParams.planetParams
        );
    }

    void Sandbox::Update(float deltaTime, bool isWindowFocused)
    {
        Engine::UpdateContext context{
            .deltaTime = deltaTime,
            .tickIndex = m_currentTick++,
            .totalTime = static_cast<float>(m_simulationTime)
        };
        m_simulationTime += static_cast<double>(deltaTime);
        Update(context, isWindowFocused);
    }

    void Sandbox::Update(const Engine::UpdateContext& context, bool isWindowFocused)
    {
        // Guard against step explosion if paused or dragging window
        const double dt = std::clamp(static_cast<double>(context.deltaTime), 0.0, 0.1);
        constexpr double turnSpeed        = 1.0;   // Turn speed in radians per second (~57 deg/s)
        constexpr double baseMoveSpeed    = 120.0; // Base movement speed in metres per second
        constexpr double zoomSpeed        = 120.0; // Zoom speed in metres per second
        constexpr double mouseSensitivity = 0.15;  // Mouse sensitivity in radians per second per pixel displacement (~0.0025 rad/px at 60 Hz)

        // Advance dynamic celestial diurnal solar cycle continuously when enabled
        if (m_enableSolarCycle)
        {
            m_sunAzimuth += m_solarTimeScale * static_cast<float>(dt);
            if (m_sunAzimuth >= Maths::TwoPi<float>)
            {
                m_sunAzimuth = std::fmod(m_sunAzimuth, Maths::TwoPi<float>);
            }
            UpdateSunDirection();
        }

        bool cameraMoved = false;

        // Process interactive input controls only when the window is active/focused
        if (isWindowFocused)
        {
            const QuatD cameraQuat  = QuatD::FromEulerAngles(m_cameraPitch, m_cameraYaw, 0.0);
            const Vec3D forward     = cameraQuat.Rotate(Vec3D::Forward());
            const Vec3D cameraRight = cameraQuat.Rotate(Vec3D::Right());

            if (m_useSpectatorCamera)
            {
                // Reset mouse tracking state while in spectator mode
                m_hasLastMousePos = false;

                // WASD free camera translation (scaled by simulation delta time)
                Vec3D moveDelta(0.0, 0.0, 0.0);
                if (GetAsyncKeyState('W') & 0x8000)
                {
                    moveDelta += forward;
                }
                if (GetAsyncKeyState('S') & 0x8000)
                {
                    moveDelta -= forward;
                }
                if (GetAsyncKeyState('D') & 0x8000)
                {
                    moveDelta += cameraRight;
                }
                if (GetAsyncKeyState('A') & 0x8000)
                {
                    moveDelta -= cameraRight;
                }
                if (GetAsyncKeyState(VK_SPACE) & 0x8000)
                {
                    moveDelta += Vec3D::Up();
                }
                if (GetAsyncKeyState(VK_LSHIFT) & 0x8000)
                {
                    moveDelta -= Vec3D::Up();
                }

                if (moveDelta.LengthSquared() > 0.0)
                {
                    m_cameraPosition += moveDelta.Normalised() * (baseMoveSpeed * dt);
                    cameraMoved = true;
                }

                // Spectator keyboard arrow key orientation controls (scaled by simulation delta time)
                if (GetAsyncKeyState(VK_LEFT) & 0x8000)
                {
                    m_cameraYaw -= turnSpeed * dt;
                    cameraMoved = true;
                }
                if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
                {
                    m_cameraYaw += turnSpeed * dt;
                    cameraMoved = true;
                }
                if (GetAsyncKeyState(VK_UP) & 0x8000)
                {
                    m_cameraPitch = std::clamp(m_cameraPitch + turnSpeed * dt, -1.50, 1.50);
                    cameraMoved = true;
                }
                if (GetAsyncKeyState(VK_DOWN) & 0x8000)
                {
                    m_cameraPitch = std::clamp(m_cameraPitch - turnSpeed * dt, -1.50, 1.50);
                    cameraMoved = true;
                }

                // Dolly forward/backward ('[' to dolly forward, ']' to dolly backward, scaled by simulation delta time)
                if (GetAsyncKeyState(VK_OEM_4) & 0x8000)
                {
                    m_cameraPosition += forward * (zoomSpeed * dt);
                    cameraMoved = true;
                }
                if (GetAsyncKeyState(VK_OEM_6) & 0x8000)
                {
                    m_cameraPosition -= forward * (zoomSpeed * dt);
                    cameraMoved = true;
                }
            }
            else if (m_character)
            {
                // Player mouse look (scaled by simulation delta time)
                POINT cursorPos;
                if (GetCursorPos(&cursorPos))
                {
                    if (m_hasLastMousePos)
                    {
                        const int deltaX = cursorPos.x - m_lastMousePos.x;
                        const int deltaY = cursorPos.y - m_lastMousePos.y;

                        if (deltaX != 0 || deltaY != 0)
                        {
                            const double deltaYaw   = static_cast<double>(deltaX) * mouseSensitivity * dt;
                            const double deltaPitch = -static_cast<double>(deltaY) * mouseSensitivity * dt;

                            const double newYaw   = m_character->GetYaw() + deltaYaw;
                            const double newPitch = std::clamp(m_character->GetPitch() + deltaPitch, MinPitch, MaxPitch);
                            m_character->SetOrientation(newYaw, newPitch);
                        }
                    }
                    m_lastMousePos = cursorPos;
                    m_hasLastMousePos = true;
                }

                // Player locomotion: walking using W, A, S, D
                // Walking is strictly horizontal (in X-Z plane) derived from character yaw,
                // keeping the body pointing directly upwards towards the positive Y axis.
                const Vec3D walkForward = m_character->GetWalkForward();
                const Vec3D walkRight   = m_character->GetWalkRight();

                Vec3D moveDir(0.0, 0.0, 0.0);
                if (GetAsyncKeyState('W') & 0x8000)
                {
                    moveDir += walkForward;
                }
                if (GetAsyncKeyState('S') & 0x8000)
                {
                    moveDir -= walkForward;
                }
                if (GetAsyncKeyState('D') & 0x8000)
                {
                    moveDir += walkRight;
                }
                if (GetAsyncKeyState('A') & 0x8000)
                {
                    moveDir -= walkRight;
                }

                if (moveDir.LengthSquared() > 0.0)
                {
                    const double walkSpeed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? (PlayerWalkSpeed * 2.0) : PlayerWalkSpeed;
                    const Vec3D horizVelocity = moveDir.Normalised() * walkSpeed;
                    m_character->SetHorizontalSpeed(horizVelocity);
                }
                else
                {
                    m_character->SetHorizontalSpeed(Vec3D{ 0.0, 0.0, 0.0 });
                }
            }

            // Toggle diagnostic text overlay visibility (F11 key)
            const bool isToggleKeyDown = (GetAsyncKeyState(VK_F11) & 0x8000) != 0;
            if (isToggleKeyDown && !m_wasOverlayToggleKeyDown)
            {
                m_renderer.ToggleOverlay();
            }
            m_wasOverlayToggleKeyDown = isToggleKeyDown;

            // Toggle spatial cell grid debug visualization (F9 key)
            const bool isCellToggleKeyDown = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
            if (isCellToggleKeyDown && !m_wasDebugCellToggleKeyDown)
            {
                ToggleDebugCells();
            }
            m_wasDebugCellToggleKeyDown = isCellToggleKeyDown;

            // Toggle active camera mode between Spectator and Character Eye (F8 key)
            const bool isCameraToggleKeyDown = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
            if (isCameraToggleKeyDown && !m_wasCameraToggleKeyDown)
            {
                ToggleCameraMode();
            }
            m_wasCameraToggleKeyDown = isCameraToggleKeyDown;

        }
        else
        {
            if (m_character)
            {
                m_character->SetHorizontalSpeed(Vec3D{ 0.0, 0.0, 0.0 });
            }
            m_wasOverlayToggleKeyDown    = false;
            m_wasDebugCellToggleKeyDown  = false;
            m_wasCameraToggleKeyDown     = false;
            m_hasLastMousePos            = false;
        }

        if (cameraMoved)
        {
            UpdateCameraVectors();
        }

        // Polymorphically update all active Base scene objects with the simulation context
        for (const auto& object : m_objects)
        {
            if (object && object->IsActive())
            {
                object->Update(context);
            }
        }

        // Synchronise spatial grid indexing for all active physical bodies
        for (const auto& body : m_bodies)
        {
            if (body && body->IsActive() && !body->IsTerrainCollider())
            {
                m_spatialGrid.UpdateEntity(body, body->GetWorldBoundingBox().GetCenter());
            }
        }

        // Resolve spatial collisions across all active bodies in the scene
        ResolveCollisions(static_cast<float>(dt));

        // Update spatial grid visibility metrics and frustum culling relative to active camera
        if (auto* activeCamera = GetActiveCamera())
        {
            m_visibleCells.clear();
            m_spatialGrid.UpdateVisibility(*activeCamera, m_visibleCells);

            for (const auto& terrain : m_terrains)
            {
                if (terrain && terrain->IsActive() && terrain->IsUsingClipmap())
                {
                    terrain->UpdateClipmap(activeCamera->GetPosition(), activeCamera->GetFrustumD());
                }
            }
        }
    }

    void Sandbox::ResolveCollisions([[maybe_unused]] float deltaTime) noexcept
    {
        if (m_bodies.empty())
        {
            return;
        }

        for (auto& body : m_bodies)
        {
            if (!body || !body->IsActive() || !body->HasCollider())
            {
                continue;
            }

            // Check this body's collider against all other active colliders in the scene
            for (const auto& other : m_bodies)
            {
                if (!other || other == body || !other->IsActive() || !other->HasCollider())
                {
                    continue;
                }

                const auto otherCollider = other->GetCollider();
                if (!otherCollider)
                {
                    continue;
                }

                if (otherCollider->IsTerrain())
                {
                    const auto* terrainCol = static_cast<const Engine::Terrain::TerrainCollider*>(otherCollider.get());
                    ResolveBodyTerrainCollision(*body, *terrainCol);
                }
                else
                {
                    if (body->Intersects(*other))
                    {
                        // Placeholder for future rigid body contact impulse response
                    }
                }
            }
        }
    }

    void Sandbox::ResolveBodyTerrainCollision(Engine::Body& body, const Engine::Terrain::TerrainCollider& terrainCollider) noexcept
    {
        Maths::Vec3D pos = body.GetPosition();
        Maths::Vec3D vel = body.GetVelocity();
        const auto collider = body.GetCollider();
        const auto& worldMatrix = body.GetWorldMatrix();

        if (!collider)
        {
            return;
        }

        if (collider->IsCapsule())
        {
            const auto* capsule = static_cast<const Engine::CapsuleCollider*>(collider.get());
            const Maths::BoundingCapsuleD worldCapsule = capsule->GetWorldBoundingCapsule(worldMatrix);
            const Engine::Terrain::TerrainContact contact = terrainCollider.TestCapsule(worldCapsule);

            if (contact.hasContact)
            {
                pos.y += contact.penetrationDepth;
                body.SetPosition(pos);
                if (vel.y < 0.0)
                {
                    vel.y = 0.0;
                }
                body.SetVelocity(vel);
                body.SetGrounded(true);
            }
            else
            {
                const double lowestY = std::min(worldCapsule.point0.y, worldCapsule.point1.y) - worldCapsule.radius;
                constexpr double maxStepDown = 0.15; // Step-down allowance of 15 cm for walking smoothly downhill
                if (body.IsGrounded() && (lowestY - contact.groundHeight) <= maxStepDown && vel.y <= 0.0)
                {
                    pos.y -= (lowestY - contact.groundHeight);
                    body.SetPosition(pos);
                    vel.y = 0.0;
                    body.SetVelocity(vel);
                    body.SetGrounded(true);
                }
                else
                {
                    body.SetGrounded(lowestY - contact.groundHeight <= 0.02);
                }
            }
            return;
        }

        if (collider->IsSphere())
        {
            const auto* sphere = static_cast<const Engine::SphereCollider*>(collider.get());
            const Maths::BoundingSphereD worldSphere = sphere->GetWorldBoundingSphere(worldMatrix);
            const Engine::Terrain::TerrainContact contact = terrainCollider.TestSphere(worldSphere.center, worldSphere.radius);

            if (contact.hasContact)
            {
                pos.y += contact.penetrationDepth;
                body.SetPosition(pos);
                if (vel.y < 0.0)
                {
                    vel.y = 0.0;
                }
                body.SetVelocity(vel);
                body.SetGrounded(true);
            }
            else
            {
                const double lowestY = worldSphere.center.y - worldSphere.radius;
                body.SetGrounded(lowestY - contact.groundHeight <= 0.02);
            }
            return;
        }

        if (collider->IsBox())
        {
            const auto* box = static_cast<const Engine::BoxCollider*>(collider.get());
            const Maths::BoundingBoxD worldBox = box->GetWorldBoundingBox(worldMatrix);
            const Maths::Vec3D center = worldBox.GetCenter();
            const double groundHeight = terrainCollider.GetHeightAt(center.x, center.z);

            if (worldBox.min.y <= groundHeight)
            {
                pos.y += (groundHeight - worldBox.min.y);
                body.SetPosition(pos);
                if (vel.y < 0.0)
                {
                    vel.y = 0.0;
                }
                body.SetVelocity(vel);
                body.SetGrounded(true);
            }
            else
            {
                body.SetGrounded(worldBox.min.y - groundHeight <= 0.02);
            }
            return;
        }
    }
}

