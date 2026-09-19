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

        // Configure summer sky clear colour and warm balanced ambient fill
        m_renderer.SetClearColor(Maths::Vec4(0.718f, 0.865f, 0.986f, 1.0f));
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
        // Generate procedural terrain geometry on CPU and upload to GPU mesh
        Engine::TerrainConfig terrainConfig;
        Engine::TerrainGenerator generator(terrainConfig);
        Engine::TerrainMeshData meshData = Engine::TerrainMesh::Generate(
            generator,
            terrainConfig.width,
            terrainConfig.depth,
            terrainConfig.resolutionX,
            terrainConfig.resolutionZ,
            terrainConfig.origin
        );

        std::shared_ptr<Renderer::Mesh> proceduralMesh;
        if (!meshData.IsEmpty() && device)
        {
            proceduralMesh = std::make_shared<Renderer::Mesh>();
            proceduralMesh->Initialise(device, meshData.vertices, meshData.indices);
        }

        auto terrain = CreateBody<Engine::TerrainObject>(proceduralMesh, terrainConfig);

        // Load compiled binary terrain mesh (.mesh) into GPU memory and verify data without drawing
        Renderer::MeshFileHeader terrainMeshHeader{};
        m_terrainMesh = Renderer::Mesh::LoadFromFile(
            device,
            "resources/environment/terrain/terrain_15km.mesh",
            &terrainMeshHeader
        );

        if (m_terrainMesh && m_terrainMesh->IsInitialised())
        {
            std::wcout << L"[Sandbox] Loaded terrain mesh successfully from terrain_15km.mesh:\n";
            std::wcout << L"          Vertices: " << m_terrainMesh->GetVertexCount() << L"\n";
            std::wcout << L"          Triangles: " << m_terrainMesh->GetTriangleCount() << L"\n";
            std::wcout << L"          Bounds: [" << terrainMeshHeader.minX << L", " << terrainMeshHeader.minY << L", " << terrainMeshHeader.minZ << L"] to ["
                       << terrainMeshHeader.maxX << L", " << terrainMeshHeader.maxY << L", " << terrainMeshHeader.maxZ << L"]\n";
            std::wcout << L"          Elevation Range: " << terrainMeshHeader.minElevation << L"m - " << terrainMeshHeader.maxElevation << L"m\n";
            std::wcout << L"          Status: Loaded into GPU memory (unrendered as requested).\n";
        }
        else
        {
            std::wcout << L"[Sandbox] Notice: terrain_15km.mesh could not be loaded.\n";
        }

        // Initialise spatial cell grid covering terrain extents
        constexpr double baseCellSize = 130.0;
        m_spatialGrid.SetBaseCellSize(baseCellSize);

        const double minX = terrainConfig.origin.x - terrainConfig.width * 0.5;
        const double maxX = terrainConfig.origin.x + terrainConfig.width * 0.5;
        const double minZ = terrainConfig.origin.z - terrainConfig.depth * 0.5;
        const double maxZ = terrainConfig.origin.z + terrainConfig.depth * 0.5;
        const double minY = terrainConfig.origin.y - terrainConfig.heightScale * 0.5;
        const double maxY = terrainConfig.origin.y + terrainConfig.heightScale * 0.5;

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

        // Initialise camera explicitly from Vec3D starting position
        SetCameraPosition(m_initialCameraPosition);

        if (m_camera)
        {
            m_visibleCells.clear();
            m_spatialGrid.UpdateVisibility(*m_camera, m_visibleCells);
        }
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

    void Sandbox::SetCameraPosition(const Maths::Vec3D& position)
    {
        m_initialCameraPosition = position;
        const double horizontalDist = std::sqrt(position.x * position.x + position.z * position.z);
        m_cameraDistance  = position.Length();
        m_cameraElevation = std::atan2(position.y, horizontalDist);
        m_cameraAzimuth   = std::atan2(position.z, position.x);
        m_cameraTarget    = Vec3D(0.0, 0.0, 0.0);

        const Vec3D viewDirection = (m_cameraTarget - position).Normalised();
        const Vec3D cameraRight   = Vec3D::Up().Cross(viewDirection).Normalised();
        const Vec3D cameraUp      = viewDirection.Cross(cameraRight).Normalised();

        if (m_camera)
        {
            m_camera->SetLookAt(position, m_cameraTarget, cameraUp);
        }
    }

    void Sandbox::UpdateCameraFromOrbit()
    {
        const double cosEle = std::cos(m_cameraElevation);
        const Vec3D cameraOffset(
            m_cameraDistance * cosEle * std::cos(m_cameraAzimuth),
            m_cameraDistance * std::sin(m_cameraElevation),
            m_cameraDistance * cosEle * std::sin(m_cameraAzimuth)
        );
        const Vec3D cameraPosition = m_cameraTarget + cameraOffset;

        // Vector pointing from eye to target
        const Vec3D viewDirection = -cameraOffset.Normalised();

        // Calculate up-vector strictly perpendicular to the view direction
        const Vec3D cameraRight = Vec3D::Up().Cross(viewDirection).Normalised();
        const Vec3D cameraUp    = viewDirection.Cross(cameraRight).Normalised();

        if (m_camera)
        {
            m_camera->SetLookAt(cameraPosition, m_cameraTarget, cameraUp);
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
        constexpr double manualOrbitSpeed = 0.75; // radians per second (~43 deg/s)
        constexpr double baseMoveSpeed   = 120.0; // metres per second

        bool cameraMoved = false;

        // Process interactive input controls only when the window is active/focused
        if (isWindowFocused)
        {
            // WASD free camera movement
            const double cosEle = std::cos(m_cameraElevation);
            const Vec3D cameraOffset(
                m_cameraDistance * cosEle * std::cos(m_cameraAzimuth),
                m_cameraDistance * std::sin(m_cameraElevation),
                m_cameraDistance * cosEle * std::sin(m_cameraAzimuth)
            );
            const Vec3D viewDirection = -cameraOffset.Normalised();
            const Vec3D cameraRight   = Vec3D::Up().Cross(viewDirection).Normalised();

            Vec3D moveDelta(0.0, 0.0, 0.0);
            if (GetAsyncKeyState('W') & 0x8000)
            {
                moveDelta += viewDirection;
            }
            if (GetAsyncKeyState('S') & 0x8000)
            {
                moveDelta -= viewDirection;
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
                m_cameraTarget += moveDelta.Normalised() * (baseMoveSpeed * dt);
                cameraMoved = true;
            }

            // Camera orbit rotation (Arrow keys)
            if (GetAsyncKeyState(VK_LEFT) & 0x8000)
            {
                m_cameraAzimuth -= manualOrbitSpeed * dt;
                cameraMoved = true;
            }
            if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
            {
                m_cameraAzimuth += manualOrbitSpeed * dt;
                cameraMoved = true;
            }
            if (GetAsyncKeyState(VK_UP) & 0x8000)
            {
                m_cameraElevation = std::clamp(m_cameraElevation + manualOrbitSpeed * dt, -1.45, 1.45);
                cameraMoved = true;
            }
            if (GetAsyncKeyState(VK_DOWN) & 0x8000)
            {
                m_cameraElevation = std::clamp(m_cameraElevation - manualOrbitSpeed * dt, -1.45, 1.45);
                cameraMoved = true;
            }

            // Zoom controls ('[' to zoom in, ']' to zoom out)
            constexpr double zoomSpeed = 120.0;
            if (GetAsyncKeyState(VK_OEM_4) & 0x8000)
            {
                m_cameraDistance = std::max(10.0, m_cameraDistance - zoomSpeed * dt);
                cameraMoved = true;
            }
            if (GetAsyncKeyState(VK_OEM_6) & 0x8000)
            {
                m_cameraDistance = std::min(900.0, m_cameraDistance + zoomSpeed * dt);
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
        }
        else
        {
            m_wasOverlayToggleKeyDown   = false;
            m_wasDebugCellToggleKeyDown = false;
        }

        if (cameraMoved)
        {
            UpdateCameraFromOrbit();
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

