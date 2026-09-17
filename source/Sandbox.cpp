// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Sandbox.h"
#include "Maths/Maths.h"

#include <algorithm>
#include <cmath>
#include <windows.h>

namespace Sandbox3D
{
    using namespace Sandbox3D::Maths;

    Sandbox::Sandbox(Renderer::Renderer& renderer, ID3D12Device* device)
        : m_renderer(renderer)
    {
        // 1. Create solid light blue 3D cube mesh and add it to sandbox render items
        // constexpr Vec4 lightBlue(0.35f, 0.65f, 0.95f, 1.0f);
        // auto cubeMesh = Renderer::Mesh::CreateCube(device, 1.0f, lightBlue);
        // AddRenderItem(std::move(cubeMesh), Mat4x4D::Identity(), "BlueCube");
        // 
        // // 2. Create cone mesh with the same styling slightly to the right of the cube (+X)
        // auto coneMesh = Renderer::Mesh::CreateCone(device, 0.5f, 1.0f, 36, lightBlue);
        // AddRenderItem(std::move(coneMesh), Mat4x4D::Translation(2.0, 0.0, 0.0), "LightBlueCone");
        // 
        // // 3. Create sphere mesh with the same styling on the opposite side of the cube (-X)
        // auto sphereMesh = Renderer::Mesh::CreateSphere(device, 0.5f, 36, 18, lightBlue);
        // AddRenderItem(std::move(sphereMesh), Mat4x4D::Translation(-2.0, 0.0, 0.0), "LightBlueSphere");

        // Initialise procedural landscape terrain
        m_terrain = std::make_unique<Terrain::Terrain>();
        m_terrain->Initialise(device);
        for (const auto& item : m_terrain->GetRenderItems())
        {
            AddRenderItem(item);
        }

        // 4. Initialise camera explicitly from Vec3D starting position
        SetCameraPosition(m_initialCameraPosition);
    }
    
    void Sandbox::AddRenderItem(Renderer::RenderItem item)
    {
        m_renderItems.push_back(std::move(item));
    }

    void Sandbox::AddRenderItem(std::shared_ptr<Renderer::Mesh> mesh, const Maths::Mat4x4D& worldMatrix, const std::string& name)
    {
        m_renderItems.push_back(Renderer::RenderItem{ std::move(mesh), worldMatrix, true, name });
    }

    void Sandbox::RemoveRenderItem(std::string_view name)
    {
        std::erase_if(m_renderItems, [name](const Renderer::RenderItem& item) { return item.name == name; });
    }

    void Sandbox::ClearRenderItems() noexcept
    {
        m_renderItems.clear();
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

        m_renderer.GetCamera().SetLookAt(position, m_cameraTarget, cameraUp);
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

        m_renderer.GetCamera().SetLookAt(cameraPosition, m_cameraTarget, cameraUp);
    }

    void Sandbox::Update(float deltaTime, bool isWindowFocused)
    {
        // Guard against step explosion if paused or dragging window
        const double dt = std::clamp(static_cast<double>(deltaTime), 0.0, 0.1);
        constexpr double manualOrbitSpeed = 1.5; // radians per second (~0.025 rad/frame at 60 FPS)
        constexpr double baseMoveSpeed   = 120.0; // metres per second

        bool cameraMoved = false;

        // Process interactive input controls only when the window is active/focused
        if (isWindowFocused)
        {
            // 1. WASD free camera movement
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

            // 2. Camera orbit rotation (Arrow keys)
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

            // 3. Zoom controls ('[' to zoom in, ']' to zoom out)
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
        }

        if (cameraMoved)
        {
            UpdateCameraFromOrbit();
        }
    }
}

