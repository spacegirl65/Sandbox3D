// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Base.h"
#include "Body.h"
#include "Camera.h"
#include "Light.h"
#include "TerrainObject.h"
#include "SpatialGrid.h"
#include "UpdateContext.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderItem.h"

#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

struct ID3D12Device;

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
        void Update(const Engine::UpdateContext& context, bool isWindowFocused = true);

        [[nodiscard]] uint64_t GetCurrentTick() const noexcept { return m_currentTick; }
        [[nodiscard]] double GetSimulationTime() const noexcept { return m_simulationTime; }

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

        // Scene Body Management (all physical bodies registered in the scene)
        void AddBody(std::shared_ptr<Engine::Body> body);
        template<typename T, typename... Args>
        std::shared_ptr<T> CreateBody(Args&&... args)
        {
            static_assert(std::is_base_of_v<Engine::Body, T>, "T must derive from Engine::Body");
            auto body = std::make_shared<T>(std::forward<Args>(args)...);
            AddBody(body);
            return body;
        }

        void RemoveBody(std::string_view name);
        void RemoveBody(uint32_t id);
        [[nodiscard]] std::shared_ptr<Engine::Body> FindBody(std::string_view name) const noexcept;
        template<typename T>
        [[nodiscard]] std::shared_ptr<T> FindBody(std::string_view name) const noexcept
        {
            return std::dynamic_pointer_cast<T>(FindBody(name));
        }
        [[nodiscard]] std::span<const std::shared_ptr<Engine::Body>> GetBodies() const noexcept { return m_bodies; }

        // Render object management (backwards compatibility)
        void AddRenderItem(Renderer::RenderItem item);
        void AddRenderItem(std::shared_ptr<Renderer::Mesh> mesh, const Maths::Mat4x4D& worldMatrix = Maths::Mat4x4D::Identity(), const std::string& name = {});
        void AddRenderItem(std::shared_ptr<Renderer::Mesh> mesh, std::shared_ptr<Renderer::Material> material, const Maths::Mat4x4D& worldMatrix = Maths::Mat4x4D::Identity(), const std::string& name = {});
        void RemoveRenderItem(std::string_view name);
        void ClearRenderItems() noexcept;

        [[nodiscard]] std::span<const Renderer::RenderItem> GetRenderItems() const noexcept;
        [[nodiscard]] Renderer::RenderItem* FindRenderItem(std::string_view name) noexcept;
        [[nodiscard]] const Renderer::RenderItem* FindRenderItem(std::string_view name) const noexcept;

        // Scene Light Management (all illumination entities registered in the scene)
        void AddLight(std::shared_ptr<Engine::Light> light);
        template<typename... Args>
        std::shared_ptr<Engine::Light> CreateLight(Args&&... args)
        {
            auto light = std::make_shared<Engine::Light>(std::forward<Args>(args)...);
            AddLight(light);
            return light;
        }

        void RemoveLight(std::string_view name);
        void RemoveLight(uint32_t id);
        [[nodiscard]] std::shared_ptr<Engine::Light> FindLight(std::string_view name) const noexcept;
        [[nodiscard]] std::span<const std::shared_ptr<Engine::Light>> GetLights() const noexcept { return m_lights; }
        [[nodiscard]] size_t GetLightCount() const noexcept { return m_lights.size(); }

        // Primary sun light access & compatibility
        [[nodiscard]] Engine::Light* GetLight() noexcept { return m_sunLight ? m_sunLight.get() : (!m_lights.empty() ? m_lights.front().get() : nullptr); }
        [[nodiscard]] const Engine::Light* GetLight() const noexcept { return m_sunLight ? m_sunLight.get() : (!m_lights.empty() ? m_lights.front().get() : nullptr); }
        [[nodiscard]] Engine::Light* GetSunLight() noexcept { return m_sunLight.get(); }
        [[nodiscard]] const Engine::Light* GetSunLight() const noexcept { return m_sunLight.get(); }
        void SetSunLight(std::shared_ptr<Engine::Light> light) { m_sunLight = std::move(light); }
        [[nodiscard]] std::span<const Renderer::GpuLight> GetLightData() const noexcept;

        // Camera access and positioning
        [[nodiscard]] Engine::Camera* GetCamera() noexcept { return m_camera.get(); }
        [[nodiscard]] const Engine::Camera* GetCamera() const noexcept { return m_camera.get(); }
        void SetCameraPosition(const Maths::Vec3D& position);
        [[nodiscard]] const Maths::Vec3D& GetInitialCameraPosition() const noexcept { return m_initialCameraPosition; }
        [[nodiscard]] const Maths::Vec3D& GetCameraTarget() const noexcept { return m_cameraTarget; }

        // Spatial grid access and debug cell visualization
        [[nodiscard]] Engine::SpatialGrid& GetSpatialGrid() noexcept { return m_spatialGrid; }
        [[nodiscard]] const Engine::SpatialGrid& GetSpatialGrid() const noexcept { return m_spatialGrid; }
        void SetShowDebugCells(bool show) noexcept { m_showDebugCells = show; }
        [[nodiscard]] bool IsDebugCellsVisible() const noexcept { return m_showDebugCells; }
        void ToggleDebugCells() noexcept { m_showDebugCells = !m_showDebugCells; }

        // Loaded terrain mesh access (unrendered)
        [[nodiscard]] const std::shared_ptr<Renderer::Mesh>& GetTerrainMesh() const noexcept { return m_terrainMesh; }

    private:
        void UpdateCameraFromOrbit();
        void RefreshRenderItemList();

    private:
        Renderer::Renderer&                         m_renderer;
        std::vector<std::shared_ptr<Engine::Base>>   m_objects;
        std::vector<std::shared_ptr<Engine::Body>>   m_bodies;
        std::vector<std::shared_ptr<Engine::Light>>  m_lights;
        std::shared_ptr<Engine::Camera>             m_camera;
        std::shared_ptr<Engine::Light>              m_sunLight;
        std::vector<Renderer::RenderItem>           m_renderItems;
        mutable std::vector<Renderer::RenderItem>   m_cachedRenderItems;
        mutable std::vector<Renderer::GpuLight>     m_cachedGpuLights;

        Engine::SpatialGrid                         m_spatialGrid{ 130.0 };
        std::vector<Engine::SpatialCell*>           m_visibleCells;
        std::shared_ptr<Renderer::Mesh>             m_debugCellMesh;
        std::shared_ptr<Renderer::Material>         m_debugCellMaterial;
        std::shared_ptr<Renderer::Mesh>             m_terrainMesh;

        Maths::Vec3D                                m_initialCameraPosition{ 0, 185.0, -370.0 };
        Maths::Vec3D                                m_cameraTarget{ 0.0, 0.0, 0.0 };
        double                                      m_cameraDistance{ 0.0 };
        double                                      m_cameraAzimuth{ 0.0 };
        double                                      m_cameraElevation{ 0.0 };
        bool                                        m_wasOverlayToggleKeyDown{ false };
        bool                                        m_wasDebugCellToggleKeyDown{ false };
        bool                                        m_showDebugCells{ false };
        uint64_t                                    m_currentTick{ 0 };
        double                                      m_simulationTime{ 0.0 };
    };
}

