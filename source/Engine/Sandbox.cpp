// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Sandbox.h"
#include "TerrainMesh.h"
#include "TerrainCollider.h"
#include "Core/Time.h"
#include "Maths/Maths.h"

#include <d3d12.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <windows.h>

namespace Sandbox3D
{
    using namespace Sandbox3D::Maths;
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
        m_lidarConfig        = Engine::TerrainConfig{};
        m_lidarConfig.width  = 15000.0;
        m_lidarConfig.depth  = 7500.0;
        m_lidarConfig.origin = Maths::Vec3D(0.0, 0.0, 0.0);

        Renderer::MeshFileHeader terrainMeshHeader{};
        Engine::TerrainMeshData lidarMeshData = Engine::TerrainMesh::GenerateFromFile(
            "resources/environment/terrain/garsdale.mesh",
            Maths::Vec3D(0.0, 0.0, 0.0),
            &terrainMeshHeader
        );

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

            m_lidarConfig.width  = meshExtentX;
            m_lidarConfig.depth  = meshExtentZ;

            Engine::TerrainMesh::ApplyProceduralPalette(
                lidarMeshData.vertices,
                m_lidarConfig,
                terrainMeshHeader.minElevation,
                terrainMeshHeader.maxElevation,
                resX,
                resZ
            );

            m_terrainMesh = std::make_shared<Renderer::Mesh>();
            m_terrainMesh->Initialise(device, lidarMeshData.vertices, lidarMeshData.indices);

            constexpr double centerElevation = 333.794;
            constexpr double scaleXZ = 1.0;
            constexpr double scaleY  = 1.0;
            m_lidarTransform = Maths::Mat4x4D::Translation(0.0, -centerElevation, 0.0);

            // Cache elevation grid for continuous terrain elevation sampling
            m_terrainElevations.resize(lidarMeshData.vertices.size());
            for (size_t i = 0; i < lidarMeshData.vertices.size(); ++i)
            {
                m_terrainElevations[i] = lidarMeshData.vertices[i].position.y;
            }
            m_terrainResX            = resX;
            m_terrainResZ            = resZ;
            m_terrainWidth           = m_lidarConfig.width;
            m_terrainDepth           = m_lidarConfig.depth;
            m_terrainCenterElevation = centerElevation;
            m_terrainScaleXZ         = scaleXZ;
            m_terrainScaleY          = scaleY;

            // Pre-bake LiDAR horizon ambient occlusion, multi-scale crevice depth, and directional horizon angle maps
            const auto lidarOcclusionMaps = Engine::TerrainMesh::ComputeLidarHorizonOcclusion(
                m_terrainElevations,
                resX,
                resZ,
                m_terrainWidth,
                m_terrainDepth
            );
            if (!lidarOcclusionMaps.IsEmpty() && device && commandQueue)
            {
                m_renderer.SetLidarOcclusionMaps(
                    device,
                    commandQueue,
                    lidarOcclusionMaps.ambientOcclusionMap.data(),
                    lidarOcclusionMaps.horizonAnglesMap0.data(),
                    lidarOcclusionMaps.horizonAnglesMap1.data(),
                    resX,
                    resZ
                );
            }

            std::wcout << L"[Sandbox] Loaded terrain successfully from garsdale.mesh:\n";
            std::wcout << L"          Vertices: " << m_terrainMesh->GetVertexCount() << L"\n";
            std::wcout << L"          Triangles: " << m_terrainMesh->GetTriangleCount() << L"\n";
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
                0.003f,
                Maths::Vec4::White(),
                Maths::Vec4(1.0f, 1.0f, 1.0f, 0.0f)
            );
            m_activeCellMesh = Renderer::Mesh::CreateWireframeBox(
                device,
                1.0f,
                0.003f,
                Maths::Vec4::White(),
                Maths::Vec4(1.0f, 1.0f, 1.0f, 0.20f)
            );
        }
        m_debugCellMaterial = Renderer::Material::CreateUnlit(Maths::Vec4::White(), "DebugCellMaterial");

        // Instantiate primary terrain body registered in the scene graph
        m_terrain = CreateBody<Engine::TerrainObject>(m_lidarConfig);

        // Generate and attach custom TerrainCollider from loaded elevation grid
        if (!m_terrainElevations.empty())
        {
            auto terrainCollider = std::make_shared<Engine::TerrainCollider>(
                m_terrainResX,
                m_terrainResZ,
                m_terrainWidth,
                m_terrainDepth,
                m_terrainElevations,
                m_terrainCenterElevation,
                m_terrainScaleXZ,
                m_terrainScaleY
            );
            m_terrain->SetCollider(terrainCollider);
            std::wcout << L"[Sandbox] Generated and attached LiDAR TerrainCollider to terrain body.\n";
        }

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
        const double groundHeight = GetTerrainHeightAt(playerSpawnX, playerSpawnZ);
        const double spawnY = groundHeight + groundClearance;
        m_character->SetPosition(Maths::Vec3D(playerSpawnX, spawnY, playerSpawnZ));

        // Orient player camera on Rise Hill facing north-northeast down the length of the dale
        constexpr double playerInitialYaw   = 0.45;  // ~26 degrees East of North
        constexpr double playerInitialPitch = -0.05; // Gently angled downward across the dale floor
        m_character->SetOrientation(playerInitialYaw, playerInitialPitch);

        std::wcout << L"[Sandbox] Spawned player character at: ("
                   << playerSpawnX << L", " << spawnY << L", " << playerSpawnZ << L") [Ground: "
                   << groundHeight << L"m, Clearance: " << groundClearance << L"m]\n";

        // Activate terrain mode (true = Garsdale LiDAR terrain, false = procedural dale terrain)
        constexpr bool defaultUseLidar = true;
        SetUseLidarTerrain(defaultUseLidar);

        // Ensure initial camera position, orientation vectors, and active camera pointer are fully synchronised
        SetCameraPosition(m_cameraPosition);
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
        std::erase_if(m_objects, [name](const std::shared_ptr<Engine::Base>& obj) {
            return obj && obj->GetName() == name;
        });
        std::erase_if(m_bodies, [name](const std::shared_ptr<Engine::Body>& body) {
            return body && body->GetName() == name;
        });
        std::erase_if(m_lights, [name](const std::shared_ptr<Engine::Light>& light) {
            return light && light->GetName() == name;
        });
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
        std::erase_if(m_objects, [id](const std::shared_ptr<Engine::Base>& obj) {
            return obj && obj->GetId() == id;
        });
        std::erase_if(m_bodies, [id](const std::shared_ptr<Engine::Body>& body) {
            return body && body->GetId() == id;
        });
        std::erase_if(m_lights, [id](const std::shared_ptr<Engine::Light>& light) {
            return light && light->GetId() == id;
        });
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

            for (const auto* cell : m_visibleCells)
            {
                if (cell && cell->IsVisible())
                {
                    // Defer the camera-occupied cell to render with m_activeCellMesh below
                    if (cell->GetCoord() == activeCoord)
                    {
                        continue;
                    }
                    m_cachedRenderItems.push_back(cell->CreateDebugRenderItem(m_debugCellMesh, m_debugCellMaterial));
                }
            }

            // Always render the cell enclosing the active camera with translucent planar faces
            if (m_activeCellMesh)
            {
                if (const auto activeCell = m_spatialGrid.FindCell(activeCoord))
                {
                    m_cachedRenderItems.push_back(activeCell->CreateDebugRenderItem(m_activeCellMesh, m_debugCellMaterial));
                }
                else
                {
                    Engine::SpatialCell tempCell(activeCoord, m_spatialGrid.GetBaseCellSize());
                    m_cachedRenderItems.push_back(tempCell.CreateDebugRenderItem(m_activeCellMesh, m_debugCellMaterial));
                }
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

    void Sandbox::RebuildSpatialGrid(const Engine::TerrainConfig& config, double minY, double maxY)
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
        const int64_t startY = static_cast<int64_t>(std::floor(minY / baseCellSize));
        const int64_t endY   = static_cast<int64_t>(std::ceil(maxY / baseCellSize));
        const int64_t startZ = static_cast<int64_t>(std::floor(minZ / baseCellSize));
        const int64_t endZ   = static_cast<int64_t>(std::ceil(maxZ / baseCellSize));

        for (int64_t iz = startZ; iz < endZ; ++iz)
        {
            for (int64_t iy = startY; iy < endY; ++iy)
            {
                for (int64_t ix = startX; ix < endX; ++ix)
                {
                    m_spatialGrid.GetOrCreateCell(Engine::CellCoord(ix, iy, iz, 0));
                }
            }
        }
    }

    void Sandbox::SetUseLidarTerrain(bool useLidar)
    {
        m_useLidarTerrain = useLidar;

        if (m_useLidarTerrain && m_terrainMesh && m_terrainMesh->IsInitialised())
        {
            if (m_terrain)
            {
                m_terrain->SetMesh(m_terrainMesh);
                m_terrain->Rebuild(m_lidarConfig);
                m_terrain->SetWorldMatrix(m_lidarTransform);
            }
            SetCameraPosition(Maths::Vec3D(0.0, 260.0, -2800.0));
            RebuildSpatialGrid(m_lidarConfig, -1000.0, 1000.0);
        }

        if (auto* activeCamera = GetActiveCamera())
        {
            m_visibleCells.clear();
            m_spatialGrid.UpdateVisibility(*activeCamera, m_visibleCells);
        }
    }

    void Sandbox::ToggleTerrainMesh()
    {
        // Procedural terrain switching disabled while procedural generation is commented out
        // SetUseLidarTerrain(!m_useLidarTerrain);
    }

    double Sandbox::GetTerrainHeightAt(double worldX, double worldZ) const noexcept
    {
        if (m_terrain)
        {
            if (auto collider = m_terrain->GetTerrainCollider())
            {
                return collider->GetHeightAt(worldX, worldZ);
            }
        }

        if (m_terrainElevations.empty() || m_terrainResX < 2 || m_terrainResZ < 2)
        {
            return 0.0;
        }

        // Convert world-space coordinates into local mesh space
        const double localX = (m_terrainScaleXZ > 0.0) ? (worldX / m_terrainScaleXZ) : worldX;
        const double localZ = (m_terrainScaleXZ > 0.0) ? (worldZ / m_terrainScaleXZ) : worldZ;

        const double halfW = m_terrainWidth * 0.5;
        const double halfD = m_terrainDepth * 0.5;

        // Map to continuous grid sample coordinates
        const double u = ((localX + halfW) / m_terrainWidth) * static_cast<double>(m_terrainResX - 1);
        const double v = ((localZ + halfD) / m_terrainDepth) * static_cast<double>(m_terrainResZ - 1);

        const double clampedU = std::clamp(u, 0.0, static_cast<double>(m_terrainResX - 1));
        const double clampedV = std::clamp(v, 0.0, static_cast<double>(m_terrainResZ - 1));

        const uint32_t ix0 = static_cast<uint32_t>(std::floor(clampedU));
        const uint32_t iz0 = static_cast<uint32_t>(std::floor(clampedV));
        const uint32_t ix1 = std::min(ix0 + 1, m_terrainResX - 1);
        const uint32_t iz1 = std::min(iz0 + 1, m_terrainResZ - 1);

        const double fx = clampedU - static_cast<double>(ix0);
        const double fz = clampedV - static_cast<double>(iz0);

        const double h00 = static_cast<double>(m_terrainElevations[iz0 * m_terrainResX + ix0]);
        const double h10 = static_cast<double>(m_terrainElevations[iz0 * m_terrainResX + ix1]);
        const double h01 = static_cast<double>(m_terrainElevations[iz1 * m_terrainResX + ix0]);
        const double h11 = static_cast<double>(m_terrainElevations[iz1 * m_terrainResX + ix1]);

        // Evaluate height along exact triangle diagonal matching clockwise mesh triangulation
        const double localHeight = (fx > fz)
            ? (h00 + fx * (h10 - h00) + fz * (h11 - h10))
            : (h00 + fz * (h01 - h00) + fx * (h11 - h01));

        // Transform local height into world height
        return (localHeight - m_terrainCenterElevation) * m_terrainScaleY;
    }

    Maths::Vec3D Sandbox::GetTerrainNormalAt(double worldX, double worldZ) const noexcept
    {
        if (m_terrain)
        {
            if (auto collider = m_terrain->GetTerrainCollider())
            {
                return collider->GetNormalAt(worldX, worldZ);
            }
        }
        return Maths::Vec3D{ 0.0, 1.0, 0.0 };
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
                   << (m_useSpectatorCamera ? L"Spectator Camera" : L"Character Eye Camera")
                   << (m_useSpectatorCamera ? L" (Gizmo Visible)\n" : L" (Gizmo Hidden)\n");
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

        // Target the character position if available, otherwise coordinate origin
        const Vec3D initialTarget = m_character
            ? Maths::Vec3D(m_character->GetPosition().x, m_character->GetPosition().y + 0.885, m_character->GetPosition().z)
            : Maths::Vec3D(0.0, 0.0, 0.0);
        const Vec3D toTarget = initialTarget - position;
        const double horizontalDist = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);

        m_cameraPitch = std::atan2(toTarget.y, horizontalDist);
        m_cameraYaw   = std::atan2(toTarget.x, toTarget.z);

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

            // Toggle active terrain mesh between LIDAR and procedural ('T' key - disabled)
            /*
            const bool isTerrainToggleKeyDown = (GetAsyncKeyState('T') & 0x8000) != 0;
            if (isTerrainToggleKeyDown && !m_wasTerrainToggleKeyDown)
            {
                ToggleTerrainMesh();
            }
            m_wasTerrainToggleKeyDown = isTerrainToggleKeyDown;
            */

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
            m_wasTerrainToggleKeyDown    = false;
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

        // Resolve spatial collisions across all active bodies in the scene
        ResolveCollisions(static_cast<float>(dt));

        // Update spatial grid visibility metrics and frustum culling relative to active camera
        if (auto* activeCamera = GetActiveCamera())
        {
            m_visibleCells.clear();
            m_spatialGrid.UpdateVisibility(*activeCamera, m_visibleCells);
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
                    const auto* terrainCol = static_cast<const Engine::TerrainCollider*>(otherCollider.get());
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

    void Sandbox::ResolveBodyTerrainCollision(Engine::Body& body, const Engine::TerrainCollider& terrainCollider) noexcept
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
            const Engine::TerrainContact contact = terrainCollider.TestCapsule(worldCapsule);

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
            const Engine::TerrainContact contact = terrainCollider.TestSphere(worldSphere.center, worldSphere.radius);

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

