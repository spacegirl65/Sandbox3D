// Copyright Ã‚Â© 2026 spacegirl65. All Rights Reserved.

#include "Sandbox.h"
#include "Collider.h"
#include "Terrain/TerrainMesh.h"
#include "Terrain/TerrainCollider.h"
#include "Terrain/TerrainMaterial.h"
#include "Core/Logger.h"
#include "Core/Time.h"
#include "Maths/Maths.h"

#include <d3d12.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <windows.h>

namespace Sandbox3D
{
    using namespace Maths;
    using Engine::LightChannel;

    Sandbox::Sandbox(Renderer::Renderer& renderer, ID3D12Device* device, ID3D12CommandQueue* commandQueue)
        : m_renderer(renderer)
    {
        (void)commandQueue;

        auto camera = m_scene.CreateObject<Engine::Camera>();
        m_scene.SetCamera(camera);
        m_renderer.SetCamera(camera.get());

        /*
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
        */

        m_renderer.SetClearColor(Maths::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
        m_renderer.SetFogColour(Maths::Vec4(0.0f, 0.0f, 0.0f, 0.0f));
        m_renderer.SetAmbientColor(Maths::Vec4(0.008f, 0.009f, 0.012f, 1.0f));
        m_atmosphereParams.rayleighParams = Maths::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        m_atmosphereParams.mieParams      = Maths::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        m_atmosphereParams.ozoneParams    = Maths::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        m_atmosphereParams.planetParams   = Maths::Vec4(0.0f, 0.0f, 22.0f, 0.0f);
        m_renderer.SetAtmosphereParameters(
            m_atmosphereParams.rayleighParams,
            m_atmosphereParams.mieParams,
            m_atmosphereParams.ozoneParams,
            m_atmosphereParams.planetParams
        );

        m_renderer.OnSceneInitialised();

        /*
        // Primary directional sun affecting all channels (terrain and character)
        m_sunLight = CreateLight("JulySummerSun");
        m_sunLight->SetChannels(LightChannel::All);
        */

        const Maths::Vec3 spaceSunVector = Engine::AtmosphereParameters::DirectionFromAzimuthElevation(
            Maths::DegToRad<float> * 37.2f,
            Maths::DegToRad<float> * 21.8f
        );
        const Maths::Vec3 spaceSunDirection = -spaceSunVector;
        constexpr float defaultSpaceSolarTemperature = 5778.0f;
        constexpr float defaultSpaceSolarIntensity   = 1.30f;

        auto sunLight = m_scene.CreateLight(
            spaceSunDirection,
            defaultSpaceSolarTemperature,
            defaultSpaceSolarIntensity,
            "Sun"
        );
        sunLight->SetChannels(LightChannel::All);
        m_scene.SetSunLight(std::move(sunLight));

        /*
        // Secondary directional ground bounce and valley point light applying exclusively to terrain
        m_earthBounceLight = CreateLight("SummerGroundBounce");
        m_earthBounceLight->SetColourTemperature(4200.0f);
        m_earthBounceLight->SetChannels(LightChannel::Terrain);

        // Synchronise initial celestial sun vector and ground bounce with ephemeris
        UpdateSunDirection();
        */

        /*
        auto summerPoint = CreateLight(
            Maths::Vec3D(0.0, 75.0, 0.0),
            350.0f,
            Maths::Vec4(1.0f, 1.0f, 1.0f, 1.0f),
            "SummerValleyPointLight"
        );
        summerPoint->SetColourTemperature(4800.0f);
        summerPoint->SetChannels(LightChannel::Terrain);
        */

        /*
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
        */

        // Initialise debug spatial cell wireframe meshes and material
        if (device)
        {
            const Maths::Vec4 gridLineColor(1.0f, 1.0f, 1.0f, 0.5f);
            m_debugCellMesh = Renderer::Mesh::CreateWireframeBox(
                device,
                1.0f,
                0.002f,
                gridLineColor,
                Maths::Vec4(1.0f, 1.0f, 1.0f, 0.0f)
            );
            m_activeCellMesh = Renderer::Mesh::CreateWireframeBox(
                device,
                1.0f,
                0.002f,
                gridLineColor,
                Maths::Vec4(1.0f, 1.0f, 1.0f, 0.15f)
            );
        }
        m_debugCellMaterial = Renderer::Material::CreateUnlit(Maths::Vec4::White(), "DebugCellMaterial");

        constexpr std::string_view characterMeshPath = "resources/entities/character.mesh";
        std::shared_ptr<Renderer::Mesh> characterMesh;
        if (device)
        {
            characterMesh = Renderer::Mesh::LoadFromFile(device, characterMeshPath);
        }

        auto character = m_scene.CreateBody<Engine::Character>(characterMesh, "PlayerCharacter");
        constexpr double groundClearance = 0.02;
        constexpr double playerSpawnX    = 0.0;
        constexpr double playerSpawnZ    = -2650.0;
        const auto primaryTerrain = m_scene.GetTerrain();
        const double groundHeight = primaryTerrain ? primaryTerrain->GetHeightAt(playerSpawnX, playerSpawnZ) : 0.0;
        const double spawnY = groundHeight + groundClearance;
        character->SetPosition(Maths::Vec3D(playerSpawnX, spawnY, playerSpawnZ));

        constexpr double playerInitialYaw   = 0.45;
        constexpr double playerInitialPitch = -0.05;
        character->SetOrientation(playerInitialYaw, playerInitialPitch);

        auto characterMaterial = std::make_shared<Renderer::Material>("Titanium");
        characterMaterial->SetShaderName("Standard");
        characterMaterial->SetAlbedo(0.82f, 0.80f, 0.78f, 1.0f);
        characterMaterial->SetRoughness(0.15f);
        characterMaterial->SetMetallic(1.0f);
        characterMaterial->SetSpecular(1.0f);
        characterMaterial->SetLightChannels(Engine::LightChannel::Default | Engine::LightChannel::Character);
        character->SetMaterial(characterMaterial);
        m_scene.SetCharacter(character);
        m_spatialGrid.InsertEntity(character, character->GetWorldBoundingBox().GetCenter());

        /*
        // Initialise spatial grid partitioning around primary terrain bounds
        if (m_terrain)
        {
            RebuildSpatialGrid(m_terrain->GetConfig(), -1000.0, 1000.0);
        }
        */

        constexpr double spectatorDistanceOffset  = -3.80;
        constexpr double spectatorLateralOffset   = 4.90;
        constexpr double spectatorVerticalOffset  = 2.58;
        constexpr double spectatorInitialYaw      = -0.508;
        constexpr double spectatorInitialPitch    = -Maths::DegToRad<double> * 12.50;

        const Maths::Vec3D spectatorCameraPosition(
            playerSpawnX + spectatorLateralOffset,
            spawnY + spectatorVerticalOffset,
            playerSpawnZ + spectatorDistanceOffset
        );

        SetCameraPosition(spectatorCameraPosition, spectatorInitialYaw, spectatorInitialPitch);
        if (auto* activeCamera = GetActiveCamera())
        {
            m_renderer.SetCamera(activeCamera);
        }

        m_renderer.SetShowGizmo(m_useSpectatorCamera);

        Core::Logger::LogInfo("[Sandbox] Scene loaded from file.");
    }

    std::span<const Renderer::RenderItem> Sandbox::GetRenderItems(bool includeDebugOverlays) const noexcept
    {
        m_cachedRenderItems.clear();
        m_scene.CollectRenderItems(m_cachedRenderItems);

        if (includeDebugOverlays && m_showDebugCells && m_debugCellMesh)
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
                    if (isCameraInsideGridCell && cell->GetCoord() == activeCoord)
                    {
                        continue;
                    }
                    m_cachedRenderItems.push_back(cell->CreateDebugRenderItem(m_debugCellMesh, m_debugCellMaterial));
                }
            }

            if (isCameraInsideGridCell && m_activeCellMesh)
            {
                m_cachedRenderItems.push_back(activeCell->CreateDebugRenderItem(m_activeCellMesh, m_debugCellMaterial));
            }
        }

        return m_cachedRenderItems;
    }

    std::span<const Renderer::GpuLight> Sandbox::GetLightData() const noexcept
    {
        const auto* activeCamera = GetActiveCamera();
        const Maths::Vec3D cameraPos = activeCamera ? activeCamera->GetPosition() : Maths::Vec3D::Zero();
        return m_scene.GetLightData(cameraPos);
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

        const auto primaryTerrain = m_scene.GetTerrain();
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

                double colMinY = 0.0;
                double colMaxY = 0.0;

                if (primaryTerrain)
                {
                    const double h0 = primaryTerrain->GetHeightAt(cellMinX, cellMinZ);
                    const double h1 = primaryTerrain->GetHeightAt(cellMaxX, cellMinZ);
                    const double h2 = primaryTerrain->GetHeightAt(cellMinX, cellMaxZ);
                    const double h3 = primaryTerrain->GetHeightAt(cellMaxX, cellMaxZ);
                    const double h4 = primaryTerrain->GetHeightAt(cellMidX, cellMidZ);

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

        for (const auto& body : m_scene.GetBodies())
        {
            if (body && !body->IsTerrainCollider())
            {
                m_spatialGrid.InsertEntity(body, body->GetWorldBoundingBox().GetCenter());
            }
        }
    }

    Engine::Camera* Sandbox::GetActiveCamera() noexcept
    {
        auto character = m_scene.GetCharacter();
        if (!m_useSpectatorCamera && character && character->GetEyeCamera())
        {
            return character->GetEyeCamera();
        }
        return m_scene.GetCamera();
    }

    const Engine::Camera* Sandbox::GetActiveCamera() const noexcept
    {
        auto character = m_scene.GetCharacter();
        if (!m_useSpectatorCamera && character && character->GetEyeCamera())
        {
            return character->GetEyeCamera();
        }
        return m_scene.GetCamera();
    }

    void Sandbox::ToggleCameraMode() noexcept
    {
        m_useSpectatorCamera = !m_useSpectatorCamera;
        m_hasLastMousePos = false;
        auto* activeCamera = GetActiveCamera();
        if (activeCamera)
        {
            if (m_playRenderer)
            {
                m_playRenderer->SetCamera(activeCamera);
            }
            m_visibleCells.clear();
            m_spatialGrid.UpdateVisibility(*activeCamera, m_visibleCells);
        }
        if (auto character = m_scene.GetCharacter())
        {
            character->SetHorizontalSpeed(Maths::Vec3D{ 0.0, 0.0, 0.0 });
        }
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

        auto character = m_scene.GetCharacter();
        const Vec3D initialTarget = (!m_useSpectatorCamera && character)
            ? Maths::Vec3D(character->GetPosition().x, character->GetPosition().y + 0.885, character->GetPosition().z)
            : Maths::Vec3D(0.0, 0.0, 0.0);
        const Vec3D toTarget = initialTarget - position;
        const double horizontalDist = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);

        m_cameraPitch = std::atan2(toTarget.y, horizontalDist);
        if (horizontalDist > Maths::DefaultEpsilon<double>)
        {
            m_cameraYaw = std::atan2(toTarget.x, toTarget.z);
        }

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
        const QuatD cameraQuat = QuatD::FromEulerAngles(m_cameraPitch, m_cameraYaw, 0.0);
        const Vec3D forward    = cameraQuat.Rotate(Vec3D::Forward());
        const Vec3D cameraUp   = cameraQuat.Rotate(Vec3D::Up());

        m_cameraTarget = m_cameraPosition + forward;

        if (auto* camera = m_scene.GetCamera())
        {
            camera->SetLookAt(m_cameraPosition, m_cameraTarget, cameraUp);
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
        const bool isDeepSpace = (m_atmosphereParams.planetParams.x <= 0.0f);
        if (isDeepSpace)
        {
            return;
        }

        const Maths::Vec3 sunVector = Engine::AtmosphereParameters::DirectionFromAzimuthElevation(
            m_sunAzimuth,
            m_sunElevation
        );
        const Maths::Vec3 lightDir = -sunVector;

        if (auto* sunLight = m_scene.GetSunLight())
        {
            const float sinElev = std::sin(m_sunElevation);
            if (sinElev > 0.0f)
            {
                sunLight->SetDirection(lightDir);
                const float intensityFactor = std::clamp(sinElev * 1.5f, 0.0f, 1.0f);
                sunLight->SetIntensity(1.05f * intensityFactor);

                const float kelvin = std::lerp(2800.0f, 5000.0f, std::clamp(sinElev * 2.0f, 0.0f, 1.0f));
                sunLight->SetColourTemperature(kelvin);
            }
            else
            {
                sunLight->SetIntensity(0.0f);
            }
        }

        if (auto* bounceLight = m_scene.GetEarthBounceLight())
        {
            bounceLight->SetDirection(sunVector);
            const float sinElev = std::sin(m_sunElevation);
            const float bounceFactor = std::clamp(sinElev * 1.5f, 0.0f, 1.0f);
            bounceLight->SetIntensity(0.15f * bounceFactor);
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
        const double dt = std::clamp(static_cast<double>(context.deltaTime), 0.0, 0.1);
        constexpr double mouseSensitivity = 0.15;

        if (m_enableSolarCycle)
        {
            m_sunAzimuth += m_solarTimeScale * static_cast<float>(dt);
            if (m_sunAzimuth >= Maths::TwoPi<float>)
            {
                m_sunAzimuth = std::fmod(m_sunAzimuth, Maths::TwoPi<float>);
            }
            UpdateSunDirection();
        }

        auto character = m_scene.GetCharacter();

        if (isWindowFocused)
        {
            if (!m_useSpectatorCamera && character)
            {
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

                            const double newYaw   = character->GetYaw() + deltaYaw;
                            const double newPitch = std::clamp(character->GetPitch() + deltaPitch, MinPitch, MaxPitch);
                            character->SetOrientation(newYaw, newPitch);
                        }
                    }
                    m_lastMousePos = cursorPos;
                    m_hasLastMousePos = true;
                }

                const Vec3D walkForward = character->GetWalkForward();
                const Vec3D walkRight   = character->GetWalkRight();

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
                    character->SetHorizontalSpeed(horizVelocity);
                }
                else
                {
                    character->SetHorizontalSpeed(Vec3D{ 0.0, 0.0, 0.0 });
                }
            }

            const bool isToggleKeyDown = (GetAsyncKeyState(VK_F11) & 0x8000) != 0;
            if (isToggleKeyDown && !m_wasOverlayToggleKeyDown)
            {
                if (m_playRenderer)
                {
                    m_playRenderer->ToggleOverlay();
                }
            }
            m_wasOverlayToggleKeyDown = isToggleKeyDown;

            const bool isCellToggleKeyDown = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
            if (isCellToggleKeyDown && !m_wasDebugCellToggleKeyDown)
            {
                ToggleDebugCells();
            }
            m_wasDebugCellToggleKeyDown = isCellToggleKeyDown;

            const bool isCameraToggleKeyDown = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
            if (isCameraToggleKeyDown && !m_wasCameraToggleKeyDown)
            {
                ToggleCameraMode();
            }
            m_wasCameraToggleKeyDown = isCameraToggleKeyDown;
        }
        else
        {
            if (character)
            {
                character->SetHorizontalSpeed(Vec3D{ 0.0, 0.0, 0.0 });
            }
            m_wasOverlayToggleKeyDown   = false;
            m_wasDebugCellToggleKeyDown = false;
            m_wasCameraToggleKeyDown    = false;
            m_hasLastMousePos           = false;
        }

        m_scene.UpdateObjects(context);
        ResolveCollisions(static_cast<float>(dt));

        if (auto* activeCamera = GetActiveCamera())
        {
            m_visibleCells.clear();
            m_spatialGrid.UpdateVisibility(*activeCamera, m_visibleCells);

            for (const auto& terrain : m_scene.GetTerrains())
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
        const auto bodies = m_scene.GetBodies();
        if (bodies.empty())
        {
            return;
        }

        for (const auto& body : bodies)
        {
            if (!body || !body->IsActive() || !body->HasCollider())
            {
                continue;
            }

            for (const auto& other : bodies)
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
                    (void)body->Intersects(*other);
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
                constexpr double maxStepDown = 0.15;
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
