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
        // 1. Initialise and register active scene camera as a Base object
        m_camera = m_renderer.GetCameraPtr();
        if (m_camera)
        {
            AddObject(m_camera);
        }

        // 2. Initialise and register active directional light as a Base object
        m_light = m_renderer.GetLightPtr();
        if (m_light)
        {
            AddObject(m_light);
        }

        // 3. Initialise and register procedural landscape terrain as a Base object
        m_terrain = CreateObject<Terrain::Terrain>();
        m_terrain->Initialise(device);

        // 4. Initialise camera explicitly from Vec3D starting position
        SetCameraPosition(m_initialCameraPosition);
    }

    void Sandbox::AddObject(std::shared_ptr<Engine::Base> object)
    {
        if (object)
        {
            m_objects.push_back(std::move(object));
        }
    }

    void Sandbox::RemoveObject(std::string_view name)
    {
        std::erase_if(m_objects, [name](const std::shared_ptr<Engine::Base>& obj) {
            return obj && obj->GetName() == name;
        });
    }

    void Sandbox::RemoveObject(uint32_t id)
    {
        std::erase_if(m_objects, [id](const std::shared_ptr<Engine::Base>& obj) {
            return obj && obj->GetId() == id;
        });
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

    std::span<const Renderer::RenderItem> Sandbox::GetRenderItems() const noexcept
    {
        m_cachedRenderItems.clear();

        // 1. Collect render items from all renderable, visible, active Base objects
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

        // 2. Append any standalone render items added directly via AddRenderItem
        for (const auto& item : m_renderItems)
        {
            if (item.mesh && item.isVisible)
            {
                m_cachedRenderItems.push_back(item);
            }
        }

        return m_cachedRenderItems;
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
        constexpr double manualOrbitSpeed = 0.75; // radians per second (~43 deg/s)
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

        // 4. Polymorphically update all active Base scene objects
        for (const auto& object : m_objects)
        {
            if (object && object->IsActive())
            {
                object->Update(deltaTime);
            }
        }
    }
}

