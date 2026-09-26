// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Sandbox.h"
#include "TerrainMesh.h"
#include "TerrainCollider.h"
#include "Maths/Maths.h"

#include <d3d12.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <windows.h>

namespace Sandbox3D
{
    using namespace Sandbox3D::Maths;

    Sandbox::Sandbox(Renderer::Renderer& renderer, ID3D12Device* device)
        : m_renderer(renderer)
    {
        // Initialise and register active scene camera as a Base object, binding non-owning pointer to Renderer
        m_camera = CreateObject<Engine::Camera>();
        m_renderer.SetCamera(m_camera.get());

        // Configure summer sky clear colour, atmospheric aerial perspective, and warm balanced ambient fill
        m_renderer.SetClearColor(Maths::Vec4(0.718f, 0.865f, 0.986f, 1.0f));
        m_renderer.SetFogColour(Maths::Vec4(0.718f, 0.865f, 0.986f, 1.0f));
        m_renderer.SetFogParams(120.0f, 1600.0f, 0.0010f);
        m_renderer.SetAmbientColor(Maths::Vec4(0.22f, 0.22f, 0.20f, 1.0f));

        // Primary directional sun)
        m_sunLight = CreateLight("JulySummerSun");
        m_sunLight->SetDirection(Maths::Vec3(-0.35f, -0.92f, -0.18f));
        m_sunLight->SetColourTemperature(5000.0f);
        m_sunLight->SetIntensity(1.05f);

        // Secondary directional bounce and valley point light commented out to isolate single primary sun
        /*
        auto earthBounce = CreateLight("SummerGroundBounce");
        earthBounce->SetDirection(Maths::Vec3(0.35f, 0.90f, 0.18f));
        earthBounce->SetColourTemperature(4200.0f);
        earthBounce->SetIntensity(0.15f);

        auto summerPoint = CreateLight(
            Maths::Vec3D(0.0, 75.0, 0.0),
            350.0f,
            Maths::Vec4(1.0f, 1.0f, 1.0f, 1.0f),
            "SummerValleyPointLight"
        );
        summerPoint->SetColourTemperature(4800.0f);
        */

        // Configure and load Garsdale LiDAR northern half terrain mesh (.mesh, 1040m x 520m)
        m_lidarConfig        = Engine::TerrainConfig{};
        m_lidarConfig.width  = 1040.0;
        m_lidarConfig.depth  = 520.0;
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
            const double subDepth = terrainMeshHeader.depth > 0.0 ? terrainMeshHeader.depth : 7500.0;
            const double scaleXZ  = 520.0 / subDepth;
            const double scaleY   = scaleXZ;
            m_lidarTransform =
                Maths::Mat4x4D::Translation(0.0, -centerElevation, 0.0) * Maths::Mat4x4D::Scale(scaleXZ, scaleY, scaleXZ);

            // Cache elevation grid for continuous terrain elevation sampling
            m_terrainElevations.resize(lidarMeshData.vertices.size());
            for (size_t i = 0; i < lidarMeshData.vertices.size(); ++i)
            {
                m_terrainElevations[i] = lidarMeshData.vertices[i].position.y;
            }
            m_terrainResX            = resX;
            m_terrainResZ            = resZ;
            m_terrainWidth           = (terrainMeshHeader.width > 0.0) ? terrainMeshHeader.width : 15000.0;
            m_terrainDepth           = (terrainMeshHeader.depth > 0.0) ? terrainMeshHeader.depth : 7500.0;
            m_terrainCenterElevation = centerElevation;
            m_terrainScaleXZ         = scaleXZ;
            m_terrainScaleY          = scaleY;

            std::wcout << L"[Sandbox] Loaded north-most half successfully from garsdale.mesh:\n";
            std::wcout << L"          Vertices: " << m_terrainMesh->GetVertexCount() << L"\n";
            std::wcout << L"          Triangles: " << m_terrainMesh->GetTriangleCount() << L"\n";
            std::wcout << L"          Sub-mesh Bounds: [" << terrainMeshHeader.minX << L", " << terrainMeshHeader.minY << L", " << terrainMeshHeader.minZ << L"] to ["
                       << terrainMeshHeader.maxX << L", " << terrainMeshHeader.maxY << L", " << terrainMeshHeader.maxZ << L"]\n";
            std::wcout << L"          Elevation Range: " << terrainMeshHeader.minElevation << L"m - " << terrainMeshHeader.maxElevation << L"m\n";
            std::wcout << L"          Status: Scaled 2x (1040m length x 520m width) and centered at origin.\n";
        }
        else
        {
            std::wcout << L"[Sandbox] Notice: garsdale.mesh could not be loaded.\n";
        }

        // Initialise debug spatial cell wireframe mesh and material
        if (device)
        {
            m_debugCellMesh = Renderer::Mesh::CreateWireframeBox(
                device,
                1.0f,
                0.003f,
                Maths::Vec4::White()
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
        constexpr double groundClearance = 0.02; // 2 cm clearance above ground turf
        const double groundHeight = GetTerrainHeightAt(0.0, 0.0);
        const double spawnY = groundHeight + groundClearance;
        m_character->SetPosition(Maths::Vec3D(0.0, spawnY, 0.0));

        std::wcout << L"[Sandbox] Spawned player character at: ("
                   << 0.0 << L", " << spawnY << L", " << 0.0 << L") [Ground: "
                   << groundHeight << L"m, Clearance: " << groundClearance << L"m]\n";

        // Activate terrain mode (true = Garsdale LiDAR terrain, false = procedural dale terrain)
        constexpr bool defaultUseLidar = true;
        SetUseLidarTerrain(defaultUseLidar);

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
            for (const auto* cell : m_visibleCells)
            {
                if (cell && cell->IsVisible())
                {
                    m_cachedRenderItems.push_back(cell->CreateDebugRenderItem(m_debugCellMesh, m_debugCellMaterial));
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
        constexpr double baseCellSize = 130.0;
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
            SetCameraPosition(Maths::Vec3D(0.0, 260.0, -460.0));
            RebuildSpatialGrid(m_lidarConfig, -65.0, 65.0);
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

        // Bilinear interpolation across the quad
        const double h0 = h00 * (1.0 - fx) + h10 * fx;
        const double h1 = h01 * (1.0 - fx) + h11 * fx;
        const double localHeight = h0 * (1.0 - fz) + h1 * fz;

        // Transform local height into world height
        return (localHeight - m_terrainCenterElevation) * m_terrainScaleY;
    }

    Engine::Camera* Sandbox::GetActiveCamera() noexcept
    {
        if (!m_useSpectatorCamera && m_character && m_character->GetCamera())
        {
            return m_character->GetCamera();
        }
        return m_camera.get();
    }

    const Engine::Camera* Sandbox::GetActiveCamera() const noexcept
    {
        if (!m_useSpectatorCamera && m_character && m_character->GetCamera())
        {
            return m_character->GetCamera();
        }
        return m_camera.get();
    }

    void Sandbox::ToggleCameraMode() noexcept
    {
        m_useSpectatorCamera = !m_useSpectatorCamera;
        m_hasLastPlayerMousePos = false;
        m_renderer.SetShowGizmo(m_useSpectatorCamera);
        auto* activeCamera = GetActiveCamera();
        if (activeCamera)
        {
            m_renderer.SetCamera(activeCamera);
            m_visibleCells.clear();
            m_spatialGrid.UpdateVisibility(*activeCamera, m_visibleCells);
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
        const double targetY = m_character ? (m_character->GetPosition().y + 0.885) : 0.0;
        const Vec3D initialTarget(0.0, targetY, 0.0);
        const Vec3D toTarget = initialTarget - position;
        const double horizontalDist = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);

        m_cameraPitch = std::atan2(toTarget.y, horizontalDist);
        m_cameraYaw   = std::atan2(toTarget.x, toTarget.z);

        UpdateCameraVectors();
    }

    void Sandbox::UpdateCameraVectors()
    {
        const double cosPitch = std::cos(m_cameraPitch);
        const double sinPitch = std::sin(m_cameraPitch);
        const double cosYaw   = std::cos(m_cameraYaw);
        const double sinYaw   = std::sin(m_cameraYaw);

        // Forward view vector from yaw and pitch (left-handed coordinate system)
        const Vec3D forward(
            cosPitch * sinYaw,
            sinPitch,
            cosPitch * cosYaw
        );

        // Calculate right and orthogonal up vectors
        const Vec3D cameraRight = Vec3D::Up().Cross(forward).Normalised();
        const Vec3D cameraUp    = forward.Cross(cameraRight).Normalised();

        m_cameraTarget = m_cameraPosition + forward;

        if (m_camera)
        {
            m_camera->SetLookAt(m_cameraPosition, m_cameraTarget, cameraUp);
        }
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
        constexpr double turnSpeed     = 1.0;   // radians per second (~57 deg/s)
        constexpr double baseMoveSpeed = 120.0; // metres per second
        constexpr double zoomSpeed     = 120.0; // metres per second

        bool cameraMoved = false;

        // Process interactive input controls only when the window is active/focused
        if (isWindowFocused)
        {
            const double cosPitch = std::cos(m_cameraPitch);
            const double sinPitch = std::sin(m_cameraPitch);
            const double cosYaw   = std::cos(m_cameraYaw);
            const double sinYaw   = std::sin(m_cameraYaw);

            const Vec3D forward(
                cosPitch * sinYaw,
                sinPitch,
                cosPitch * cosYaw
            );
            const Vec3D cameraRight = Vec3D::Up().Cross(forward).Normalised();

            if (m_useSpectatorCamera)
            {
                // WASD free camera translation
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

                // Camera orientation controls: rotate about camera itself (position fixed)
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

                // Dolly forward/backward ('[' to dolly forward, ']' to dolly backward)
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
                // Player Mode: mouse look & keyboard arrow key fallback
                POINT cursorPos;
                if (GetCursorPos(&cursorPos))
                {
                    if (m_hasLastPlayerMousePos)
                    {
                        const int deltaX = cursorPos.x - m_lastPlayerMousePos.x;
                        const int deltaY = cursorPos.y - m_lastPlayerMousePos.y;

                        if (deltaX != 0 || deltaY != 0)
                        {
                            constexpr double mouseSensitivity = 0.0025; // radians per pixel
                            const double deltaYaw   = static_cast<double>(deltaX) * mouseSensitivity;
                            const double deltaPitch = -static_cast<double>(deltaY) * mouseSensitivity;

                            const double newYaw   = m_character->GetYaw() + deltaYaw;
                            const double newPitch = std::clamp(m_character->GetPitch() + deltaPitch, MinPitch, MaxPitch);
                            m_character->SetOrientation(newYaw, newPitch);
                        }
                    }
                    m_lastPlayerMousePos = cursorPos;
                    m_hasLastPlayerMousePos = true;
                }

                // Keyboard arrow keys fallback for looking around in player mode
                double keyDeltaYaw   = 0.0;
                double keyDeltaPitch = 0.0;
                if (GetAsyncKeyState(VK_LEFT) & 0x8000)
                {
                    keyDeltaYaw -= turnSpeed * dt;
                }
                if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
                {
                    keyDeltaYaw += turnSpeed * dt;
                }
                if (GetAsyncKeyState(VK_UP) & 0x8000)
                {
                    keyDeltaPitch += turnSpeed * dt;
                }
                if (GetAsyncKeyState(VK_DOWN) & 0x8000)
                {
                    keyDeltaPitch -= turnSpeed * dt;
                }

                if (keyDeltaYaw != 0.0 || keyDeltaPitch != 0.0)
                {
                    const double newYaw   = m_character->GetYaw() + keyDeltaYaw;
                    const double newPitch = std::clamp(m_character->GetPitch() + keyDeltaPitch, MinPitch, MaxPitch);
                    m_character->SetOrientation(newYaw, newPitch);
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
            m_wasOverlayToggleKeyDown    = false;
            m_wasDebugCellToggleKeyDown  = false;
            m_wasTerrainToggleKeyDown    = false;
            m_wasCameraToggleKeyDown     = false;
            m_hasLastPlayerMousePos      = false;
        }

        if (cameraMoved)
        {
            UpdateCameraVectors();
        }

        // Update spatial grid visibility metrics and frustum culling relative to active camera
        if (auto* activeCamera = GetActiveCamera())
        {
            m_visibleCells.clear();
            m_spatialGrid.UpdateVisibility(*activeCamera, m_visibleCells);
        }

        // Polymorphically update all active Base scene objects with the simulation context
        for (const auto& object : m_objects)
        {
            if (object && object->IsActive())
            {
                object->Update(context);
            }
        }
    }
}

