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
        constexpr Vec4 lightBlue(0.35f, 0.65f, 0.95f, 1.0f);
        auto cubeMesh = Renderer::Mesh::CreateCube(device, 1.0f, lightBlue);
        AddRenderItem(std::move(cubeMesh), Mat4x4D::Identity(), "BlueCube");

        // 2. Create cone mesh with the same styling slightly to the right of the cube (+X)
        auto coneMesh = Renderer::Mesh::CreateCone(device, 0.5f, 1.0f, 36, lightBlue);
        AddRenderItem(std::move(coneMesh), Mat4x4D::Translation(2.0, 0.0, 0.0), "LightBlueCone");

        // 3. Create sphere mesh with the same styling on the opposite side of the cube (-X)
        auto sphereMesh = Renderer::Mesh::CreateSphere(device, 0.5f, 36, 18, lightBlue);
        AddRenderItem(std::move(sphereMesh), Mat4x4D::Translation(-2.0, 0.0, 0.0), "LightBlueSphere");

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
        UpdateCameraFromOrbit();
    }

    void Sandbox::UpdateCameraFromOrbit()
    {
        const double cosEle = std::cos(m_cameraElevation);
        const Vec3D cameraPosition(
            m_cameraDistance * cosEle * std::cos(m_cameraAzimuth),
            m_cameraDistance * std::sin(m_cameraElevation),
            m_cameraDistance * cosEle * std::sin(m_cameraAzimuth)
        );
        const Vec3D cameraTarget(0.0, 0.0, 0.0);

        // Vector pointing from eye to target
        const Vec3D viewDirection = (cameraTarget - cameraPosition).Normalised();

        // Calculate up-vector strictly perpendicular to the view direction
        const Vec3D cameraRight = Vec3D::Up().Cross(viewDirection).Normalised();
        const Vec3D cameraUp    = viewDirection.Cross(cameraRight).Normalised();

        m_renderer.GetCamera().SetLookAt(cameraPosition, cameraTarget, cameraUp);
    }

    void Sandbox::Update(float deltaTime)
    {
        // Guard against step explosion if paused or dragging window
        const double dt = std::clamp(static_cast<double>(deltaTime), 0.0, 0.1);
        constexpr double autoOrbitSpeed   = 0.6; // radians per second (~0.01 rad/frame at 60 FPS)
        constexpr double manualOrbitSpeed = 1.5; // radians per second (~0.025 rad/frame at 60 FPS)

        bool cameraMoved = false;

        // Space bar toggles auto-orbit
        const bool spaceIsDown = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
        if (spaceIsDown && !m_spaceWasPressed)
        {
            m_autoOrbit = !m_autoOrbit;
        }
        m_spaceWasPressed = spaceIsDown;

        if (m_autoOrbit)
        {
            m_cameraAzimuth += autoOrbitSpeed * dt;
            cameraMoved = true;
        }

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
        if (GetAsyncKeyState('R') & 0x8000)
        {
            SetCameraPosition(m_initialCameraPosition);
            m_autoOrbit = false;
        }

        if (cameraMoved)
        {
            UpdateCameraFromOrbit();
        }
    }
}

