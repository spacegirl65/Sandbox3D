// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Base.h"
#include "Body.h"
#include "Camera.h"
#include "Character.h"
#include "Light.h"
#include "Terrain/TerrainObject.h"
#include "UpdateContext.h"
#include "Renderer/RenderItem.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace Sandbox3D::Engine
{
    // Represents a 3D scene responsible solely for object creation, storage, lookup, and lifetime management
    class Scene final
    {
    public:
        explicit Scene(std::string name = "Untitled Scene");
        ~Scene() = default;

        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;
        Scene(Scene&&) noexcept = default;
        Scene& operator=(Scene&&) noexcept = default;

        [[nodiscard]] const std::string& GetName() const noexcept { return m_name; }
        void SetName(std::string name) noexcept { m_name = std::move(name); }

        // Base object creation, removal, and lookup
        void AddObject(std::shared_ptr<Base> object);
        template<typename T, typename... Args>
        std::shared_ptr<T> CreateObject(Args&&... args)
        {
            auto object = std::make_shared<T>(std::forward<Args>(args)...);
            AddObject(object);
            return object;
        }
        void RemoveObject(std::string_view name);
        void RemoveObject(uint32_t id);
        [[nodiscard]] std::span<const std::shared_ptr<Base>> GetObjects() const noexcept { return m_objects; }
        [[nodiscard]] std::shared_ptr<Base> FindObject(std::string_view name) const noexcept;
        template<typename T>
        [[nodiscard]] std::shared_ptr<T> FindObject(std::string_view name) const noexcept
        {
            return std::dynamic_pointer_cast<T>(FindObject(name));
        }

        // Physical body creation, removal, and lookup
        void AddBody(std::shared_ptr<Body> body);
        template<typename T, typename... Args>
        std::shared_ptr<T> CreateBody(Args&&... args)
        {
            static_assert(std::is_base_of_v<Body, T>, "T must derive from Engine::Body");
            auto body = std::make_shared<T>(std::forward<Args>(args)...);
            AddBody(body);
            return body;
        }
        void RemoveBody(std::string_view name);
        void RemoveBody(uint32_t id);
        [[nodiscard]] std::span<const std::shared_ptr<Body>> GetBodies() const noexcept { return m_bodies; }
        [[nodiscard]] std::shared_ptr<Body> FindBody(std::string_view name) const noexcept;
        template<typename T>
        [[nodiscard]] std::shared_ptr<T> FindBody(std::string_view name) const noexcept
        {
            return std::dynamic_pointer_cast<T>(FindBody(name));
        }

        // Explicit render item registration and collection
        void AddRenderItem(Renderer::RenderItem item);
        void AddRenderItem(std::shared_ptr<Renderer::Mesh> mesh, const Maths::Mat4x4D& worldMatrix = Maths::Mat4x4D::Identity(), const std::string& name = {});
        void AddRenderItem(std::shared_ptr<Renderer::Mesh> mesh, std::shared_ptr<Renderer::Material> material, const Maths::Mat4x4D& worldMatrix = Maths::Mat4x4D::Identity(), const std::string& name = {});
        void RemoveRenderItem(std::string_view name);
        void ClearRenderItems() noexcept;
        void CollectRenderItems(std::vector<Renderer::RenderItem>& outItems) const;
        [[nodiscard]] std::span<const Renderer::RenderItem> GetRenderItems() const noexcept;
        [[nodiscard]] Renderer::RenderItem* FindRenderItem(std::string_view name) noexcept;
        [[nodiscard]] const Renderer::RenderItem* FindRenderItem(std::string_view name) const noexcept;

        // Light creation, removal, and lookup
        void AddLight(std::shared_ptr<Light> light);
        template<typename... Args>
        std::shared_ptr<Light> CreateLight(Args&&... args)
        {
            auto light = std::make_shared<Light>(std::forward<Args>(args)...);
            AddLight(light);
            return light;
        }
        void RemoveLight(std::string_view name);
        void RemoveLight(uint32_t id);
        [[nodiscard]] std::span<const std::shared_ptr<Light>> GetLights() const noexcept { return m_lights; }
        [[nodiscard]] size_t GetLightCount() const noexcept { return m_lights.size(); }
        [[nodiscard]] std::shared_ptr<Light> FindLight(std::string_view name) const noexcept;
        [[nodiscard]] Light* GetLight() noexcept { return m_sunLight ? m_sunLight.get() : (!m_lights.empty() ? m_lights.front().get() : nullptr); }
        [[nodiscard]] const Light* GetLight() const noexcept { return m_sunLight ? m_sunLight.get() : (!m_lights.empty() ? m_lights.front().get() : nullptr); }
        [[nodiscard]] Light* GetSunLight() noexcept { return m_sunLight.get(); }
        [[nodiscard]] const Light* GetSunLight() const noexcept { return m_sunLight.get(); }
        void SetSunLight(std::shared_ptr<Light> sunLight) noexcept { m_sunLight = std::move(sunLight); }
        [[nodiscard]] Light* GetEarthBounceLight() noexcept { return m_earthBounceLight.get(); }
        void SetEarthBounceLight(std::shared_ptr<Light> bounceLight) noexcept { m_earthBounceLight = std::move(bounceLight); }
        [[nodiscard]] std::span<const Renderer::GpuLight> GetLightData(const Maths::Vec3D& cameraPosition) const noexcept;

        // Terrain creation, removal, and lookup
        void AddTerrain(std::shared_ptr<Terrain::TerrainObject> terrain);
        template<typename... Args>
        std::shared_ptr<Terrain::TerrainObject> CreateTerrain(Args&&... args)
        {
            auto terrain = std::make_shared<Terrain::TerrainObject>(std::forward<Args>(args)...);
            AddTerrain(terrain);
            return terrain;
        }
        void RemoveTerrain(std::string_view name);
        void RemoveTerrain(uint32_t id);
        [[nodiscard]] std::span<const std::shared_ptr<Terrain::TerrainObject>> GetTerrains() const noexcept { return m_terrains; }
        [[nodiscard]] size_t GetTerrainCount() const noexcept { return m_terrains.size(); }
        [[nodiscard]] std::shared_ptr<Terrain::TerrainObject> FindTerrain(std::string_view name) const noexcept;
        [[nodiscard]] std::shared_ptr<Terrain::TerrainObject> GetTerrain() const noexcept { return m_terrain ? m_terrain : (!m_terrains.empty() ? m_terrains.front() : nullptr); }
        void SetPrimaryTerrain(std::shared_ptr<Terrain::TerrainObject> terrain) noexcept { m_terrain = std::move(terrain); }

        // Primary scene entity references
        void SetCamera(std::shared_ptr<Camera> camera) noexcept { m_camera = std::move(camera); }
        [[nodiscard]] Camera* GetCamera() noexcept { return m_camera.get(); }
        [[nodiscard]] const Camera* GetCamera() const noexcept { return m_camera.get(); }

        void SetCharacter(std::shared_ptr<Character> character) noexcept { m_character = std::move(character); }
        [[nodiscard]] std::shared_ptr<Character> GetCharacter() const noexcept { return m_character; }

        void UpdateObjects(const UpdateContext& context);

    private:
        std::string                                          m_name;
        std::vector<std::shared_ptr<Base>>                   m_objects;
        std::vector<std::shared_ptr<Body>>                   m_bodies;
        std::vector<std::shared_ptr<Terrain::TerrainObject>> m_terrains;
        std::vector<std::shared_ptr<Light>>                  m_lights;
        std::shared_ptr<Camera>                              m_camera;
        std::shared_ptr<Character>                           m_character;
        std::shared_ptr<Light>                               m_sunLight;
        std::shared_ptr<Light>                               m_earthBounceLight;
        std::shared_ptr<Terrain::TerrainObject>            m_terrain;

        std::vector<Renderer::RenderItem>                    m_renderItems;
        mutable std::vector<Renderer::RenderItem>            m_cachedRenderItems;
        mutable std::vector<Renderer::GpuLight>              m_cachedGpuLights;
    };
}
