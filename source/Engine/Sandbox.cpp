// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Sandbox.h"
#include "TerrainMesh.h"
#include "Maths/Maths.h"

#include <d3d12.h>
#include <algorithm>
#include <cmath>
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

        // Primary directional sun: warm summer sun (5000K colour temperature, softened intensity)
        m_sunLight = CreateLight("JulySummerSun");
        m_sunLight->SetDirection(Maths::Vec3(-0.35f, -0.92f, -0.18f));
        m_sunLight->SetColourTemperature(5000.0f);
        m_sunLight->SetIntensity(1.05f);

        // Secondary directional bounce: subtle warm terrain reflection (4200K)
        auto earthBounce = CreateLight("SummerGroundBounce");
        earthBounce->SetDirection(Maths::Vec3(0.35f, 0.90f, 0.18f));
        earthBounce->SetColourTemperature(4200.0f);
        earthBounce->SetIntensity(0.15f);

        // Gentle valley accent point light (4800K, reduced from 1.3 to avoid overexposure)
        auto summerPoint = CreateLight(
            Maths::Vec3D(0.0, 75.0, 0.0),
            350.0f,
            Maths::Vec4(1.0f, 1.0f, 1.0f, 1.0f),
            "SummerValleyPointLight"
        );
        summerPoint->SetColourTemperature(4800.0f);
        // Configure and generate procedural terrain mesh with natural isotropic dimensions (520m x 520m)
        m_proceduralConfig = Engine::TerrainConfig{};
        Engine::TerrainGenerator generator(m_proceduralConfig);
        Engine::TerrainMeshData proceduralMeshData = Engine::TerrainMesh::Generate(
            generator,
            m_proceduralConfig.width,
            m_proceduralConfig.depth,
            m_proceduralConfig.resolutionX,
            m_proceduralConfig.resolutionZ,
            m_proceduralConfig.origin
        );

        if (!proceduralMeshData.IsEmpty() && device)
        {
            m_proceduralMesh = std::make_shared<Renderer::Mesh>();
            m_proceduralMesh->Initialise(device, proceduralMeshData.vertices, proceduralMeshData.indices);
        }

        // Configure and load Garsdale LiDAR terrain mesh (.mesh), extracting northern half (1040m x 520m)
        m_lidarConfig        = Engine::TerrainConfig{};
        m_lidarConfig.width  = 1040.0;
        m_lidarConfig.depth  = 520.0;
        m_lidarConfig.origin = Maths::Vec3D(0.0, 0.0, 0.0);

        Renderer::MeshFileHeader fullMeshHeader{};
        Engine::TerrainMeshData fullLidarMeshData = Engine::TerrainMesh::GenerateFromFile(
            "resources/environment/terrain/garsdale.mesh",
            Maths::Vec3D(0.0, 0.0, 0.0),
            &fullMeshHeader
        );

        Renderer::MeshFileHeader terrainMeshHeader{};
        Engine::TerrainMeshData lidarMeshData = Engine::TerrainMesh::ExtractNorthHalf(
            fullLidarMeshData,
            fullMeshHeader,
            &terrainMeshHeader
        );

        if (!lidarMeshData.IsEmpty() && device)
        {
            Engine::TerrainMesh::ApplyProceduralPalette(
                lidarMeshData.vertices,
                m_lidarConfig,
                terrainMeshHeader.minElevation,
                terrainMeshHeader.maxElevation
            );

            m_terrainMesh = std::make_shared<Renderer::Mesh>();
            m_terrainMesh->Initialise(device, lidarMeshData.vertices, lidarMeshData.indices);

            constexpr double centerElevation = 333.794;
            const double subDepth = terrainMeshHeader.depth > 0.0 ? terrainMeshHeader.depth : 7500.0;
            const double scaleXZ  = 520.0 / subDepth;
            const double scaleY   = scaleXZ;
            m_lidarTransform =
                Maths::Mat4x4D::Translation(0.0, -centerElevation, 0.0) * Maths::Mat4x4D::Scale(scaleXZ, scaleY, scaleXZ);

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

        // Activate terrain mode (true = Garsdale LiDAR terrain, false = procedural dale terrain)
        constexpr bool defaultUseLidar = true;
        SetUseLidarTerrain(defaultUseLidar);
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
        const Maths::Vec3D cameraPos = m_camera ? m_camera->GetPosition() : Maths::Vec3D::Zero();

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
            std::wcout << L"[Sandbox] Active terrain: LIDAR Terrain (1040m length x 520m width)\n";
        }
        else if (m_proceduralMesh && m_proceduralMesh->IsInitialised())
        {
            m_useLidarTerrain = false;
            if (m_terrain)
            {
                m_terrain->SetMesh(m_proceduralMesh);
                m_terrain->Rebuild(m_proceduralConfig);
                m_terrain->SetWorldMatrix(Maths::Mat4x4D::Identity());
            }
            SetCameraPosition(Maths::Vec3D(0.0, 185.0, -370.0));
            RebuildSpatialGrid(m_proceduralConfig, -52.0, 52.0);
            std::wcout << L"[Sandbox] Active terrain: Procedural Terrain (520m length x 520m width)\n";
        }

        if (m_camera)
        {
            m_visibleCells.clear();
            m_spatialGrid.UpdateVisibility(*m_camera, m_visibleCells);
        }
    }

    void Sandbox::ToggleTerrainMesh()
    {
        SetUseLidarTerrain(!m_useLidarTerrain);
    }

    void Sandbox::SetCameraPosition(const Maths::Vec3D& position)
    {
        m_initialCameraPosition = position;
        m_cameraPosition        = position;

        // Target the coordinate origin by default
        const Vec3D initialTarget(0.0, 0.0, 0.0);
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

            // Toggle diagnostic text overlay visibility (F11 or Home key for laptop keyboards)
            const bool isToggleKeyDown = ((GetAsyncKeyState(VK_F11) & 0x8000) != 0) ||
                                         ((GetAsyncKeyState(VK_HOME) & 0x8000) != 0);
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

            // Toggle active terrain mesh between LIDAR and procedural ('T' key)
            const bool isTerrainToggleKeyDown = (GetAsyncKeyState('T') & 0x8000) != 0;
            if (isTerrainToggleKeyDown && !m_wasTerrainToggleKeyDown)
            {
                ToggleTerrainMesh();
            }
            m_wasTerrainToggleKeyDown = isTerrainToggleKeyDown;
        }
        else
        {
            m_wasOverlayToggleKeyDown   = false;
            m_wasDebugCellToggleKeyDown = false;
            m_wasTerrainToggleKeyDown   = false;
        }

        if (cameraMoved)
        {
            UpdateCameraVectors();
        }

        // Update spatial grid visibility metrics and frustum culling relative to active camera
        if (m_camera)
        {
            m_visibleCells.clear();
            m_spatialGrid.UpdateVisibility(*m_camera, m_visibleCells);
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

