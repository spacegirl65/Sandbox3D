// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Base.h"
#include "Camera.h"
#include "Light.h"
#include "Terrain.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderItem.h"

#include <d3d12.h>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace Sandbox3D
{
    // Encapsulates 3D sandbox scene objects, input handling, and update simulation logic
    class Sandbox final
    {
    public:
        Sandbox(Renderer::Renderer& renderer, ID3D12Device* device);
        ~Sandbox() = default;

        Sandbox(const Sandbox&) = delete;
        Sandbox& operator=(const Sandbox&) = delete;
        Sandbox(Sandbox&&) noexcept = delete;
        Sandbox& operator=(Sandbox&&) noexcept = delete;

        void Update(float deltaTime, bool isWindowFocused = true);

        // Scene Base Object Management
        void AddObject(std::shared_ptr<Engine::Base> object);
        template<typename T, typename... Args>
        std::shared_ptr<T> CreateObject(Args&&... args)
        {
            auto object = std::make_shared<T>(std::forward<Args>(args)...);
            AddObject(object);
            return object;
        }

        void RemoveObject(std::string_view name);
        void RemoveObject(uint32_t id);
        [[nodiscard]] std::shared_ptr<Engine::Base> FindObject(std::string_view name) const noexcept;
        template<typename T>
        [[nodiscard]] std::shared_ptr<T> FindObject(std::string_view name) const noexcept
        {
            return std::dynamic_pointer_cast<T>(FindObject(name));
        }
        [[nodiscard]] std::span<const std::shared_ptr<Engine::Base>> GetObjects() const noexcept { return m_objects; }

        // Render object management (backwards compatibility)
        void AddRenderItem(Renderer::RenderItem item);
        void AddRenderItem(std::shared_ptr<Renderer::Mesh> mesh, const Maths::Mat4x4D& worldMatrix = Maths::Mat4x4D::Identity(), const std::string& name = {});
        void RemoveRenderItem(std::string_view name);
        void ClearRenderItems() noexcept;

        [[nodiscard]] std::span<const Renderer::RenderItem> GetRenderItems() const noexcept;
        [[nodiscard]] Renderer::RenderItem* FindRenderItem(std::string_view name) noexcept;
        [[nodiscard]] const Renderer::RenderItem* FindRenderItem(std::string_view name) const noexcept;

        // Terrain scenery access
        [[nodiscard]] Terrain::Terrain* GetTerrain() noexcept { return m_terrain.get(); }
        [[nodiscard]] const Terrain::Terrain* GetTerrain() const noexcept { return m_terrain.get(); }

        // Directional light access
        [[nodiscard]] Engine::Light* GetLight() noexcept { return m_light.get(); }
        [[nodiscard]] const Engine::Light* GetLight() const noexcept { return m_light.get(); }
        [[nodiscard]] std::shared_ptr<Engine::Light> GetLightPtr() const noexcept { return m_light; }

        // Camera access and positioning
        [[nodiscard]] Engine::Camera* GetCamera() noexcept { return m_camera.get(); }
        [[nodiscard]] const Engine::Camera* GetCamera() const noexcept { return m_camera.get(); }
        void SetCameraPosition(const Maths::Vec3D& position);
        [[nodiscard]] const Maths::Vec3D& GetInitialCameraPosition() const noexcept { return m_initialCameraPosition; }
        [[nodiscard]] const Maths::Vec3D& GetCameraTarget() const noexcept { return m_cameraTarget; }

    private:
        void UpdateCameraFromOrbit();
        void RefreshRenderItemList();

    private:
        Renderer::Renderer&                         m_renderer;
        std::vector<std::shared_ptr<Engine::Base>>   m_objects;
        std::shared_ptr<Terrain::Terrain>           m_terrain;
        std::shared_ptr<Engine::Camera>             m_camera;
        std::shared_ptr<Engine::Light>              m_light;
        std::vector<Renderer::RenderItem>           m_renderItems;
        mutable std::vector<Renderer::RenderItem>   m_cachedRenderItems;

        Maths::Vec3D                                m_initialCameraPosition{ 0, 185.0, -370.0 };
        Maths::Vec3D                                m_cameraTarget{ 0.0, 0.0, 0.0 };
        double                                      m_cameraDistance{ 0.0 };
        double                                      m_cameraAzimuth{ 0.0 };
        double                                      m_cameraElevation{ 0.0 };
    };
}

