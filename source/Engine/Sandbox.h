// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "AtmosphereConstants.h"
#include "Scene.h"
#include "SpatialGrid.h"
#include "Terrain/TerrainCollider.h"
#include "Terrain/TerrainConfig.h"
#include "UpdateContext.h"
#include "Maths/MathsCommon.h"
#include "Renderer/Renderer.h"

#include <memory>
#include <span>
#include <string_view>
#include <vector>

struct ID3D12Device;

namespace Sandbox3D
{
    // Encapsulates 3D sandbox simulation, physics, spatial partitioning, solar cycle, and input handling
    class Sandbox final
    {
    public:
        Sandbox(Renderer::Renderer& renderer, ID3D12Device* device, ID3D12CommandQueue* commandQueue = nullptr);
        ~Sandbox() = default;

        Sandbox(const Sandbox&) = delete;
        Sandbox& operator=(const Sandbox&) = delete;
        Sandbox(Sandbox&&) noexcept = delete;
        Sandbox& operator=(Sandbox&&) noexcept = delete;

        // Primary simulation update cycles
        void Update(float deltaTime, bool isWindowFocused = true);
        void Update(const Engine::UpdateContext& context, bool isWindowFocused = true);

        // Active scene accessors
        [[nodiscard]] Engine::Scene& GetScene() noexcept { return m_scene; }
        [[nodiscard]] const Engine::Scene& GetScene() const noexcept { return m_scene; }

        // Simulation telemetry accessors
        [[nodiscard]] uint64_t GetCurrentTick() const noexcept { return m_currentTick; }
        [[nodiscard]] double GetSimulationTime() const noexcept { return m_simulationTime; }

        // Scene object inspection accessors delegated to Scene
        [[nodiscard]] std::span<const std::shared_ptr<Engine::Base>> GetObjects() const noexcept { return m_scene.GetObjects(); }
        [[nodiscard]] std::shared_ptr<Engine::Base> FindObject(std::string_view name) const noexcept { return m_scene.FindObject(name); }
        template<typename T>
        [[nodiscard]] std::shared_ptr<T> FindObject(std::string_view name) const noexcept
        {
            return m_scene.FindObject<T>(name);
        }

        [[nodiscard]] std::span<const std::shared_ptr<Engine::Body>> GetBodies() const noexcept { return m_scene.GetBodies(); }
        [[nodiscard]] std::shared_ptr<Engine::Body> FindBody(std::string_view name) const noexcept { return m_scene.FindBody(name); }
        template<typename T>
        [[nodiscard]] std::shared_ptr<T> FindBody(std::string_view name) const noexcept
        {
            return m_scene.FindBody<T>(name);
        }

        // Render item inspection accessors
        [[nodiscard]] std::span<const Renderer::RenderItem> GetRenderItems(bool includeDebugOverlays = true) const noexcept;
        void SetPlayRenderer(Renderer::Renderer* playRenderer) noexcept { m_playRenderer = playRenderer; }
        [[nodiscard]] Renderer::RenderItem* FindRenderItem(std::string_view name) noexcept { return m_scene.FindRenderItem(name); }
        [[nodiscard]] const Renderer::RenderItem* FindRenderItem(std::string_view name) const noexcept { return m_scene.FindRenderItem(name); }

        // Scene light inspection accessors
        [[nodiscard]] std::span<const std::shared_ptr<Engine::Light>> GetLights() const noexcept { return m_scene.GetLights(); }
        [[nodiscard]] size_t GetLightCount() const noexcept { return m_scene.GetLightCount(); }
        [[nodiscard]] std::shared_ptr<Engine::Light> FindLight(std::string_view name) const noexcept { return m_scene.FindLight(name); }
        [[nodiscard]] Engine::Light* GetLight() noexcept { return m_scene.GetLight(); }
        [[nodiscard]] const Engine::Light* GetLight() const noexcept { return m_scene.GetLight(); }
        [[nodiscard]] Engine::Light* GetSunLight() noexcept { return m_scene.GetSunLight(); }
        [[nodiscard]] const Engine::Light* GetSunLight() const noexcept { return m_scene.GetSunLight(); }
        [[nodiscard]] std::span<const Renderer::GpuLight> GetLightData() const noexcept;

        // Celestial solar ephemeris and atmospheric illumination inspection
        [[nodiscard]] float GetSunAzimuth() const noexcept { return m_sunAzimuth; }
        [[nodiscard]] float GetSunElevation() const noexcept { return m_sunElevation; }
        [[nodiscard]] float GetSunAzimuthDegrees() const noexcept { return Maths::RadToDeg<float> * m_sunAzimuth; }
        [[nodiscard]] float GetSunElevationDegrees() const noexcept { return Maths::RadToDeg<float> * m_sunElevation; }
        [[nodiscard]] bool IsSolarCycleEnabled() const noexcept { return m_enableSolarCycle; }
        [[nodiscard]] float GetSolarTimeScale() const noexcept { return m_solarTimeScale; }
        [[nodiscard]] const Engine::AtmosphereParameters& GetAtmosphereParameters() const noexcept { return m_atmosphereParams; }
        [[nodiscard]] const Engine::AtmosphereParameters& GetAtmosphereParams() const noexcept { return m_atmosphereParams; }

        // Player character entity access
        [[nodiscard]] std::shared_ptr<Engine::Character> GetCharacter() const noexcept { return m_scene.GetCharacter(); }

        // Camera inspection accessors
        [[nodiscard]] Engine::Camera* GetCamera() noexcept { return m_scene.GetCamera(); }
        [[nodiscard]] const Engine::Camera* GetCamera() const noexcept { return m_scene.GetCamera(); }
        [[nodiscard]] Engine::Camera* GetActiveCamera() noexcept;
        [[nodiscard]] const Engine::Camera* GetActiveCamera() const noexcept;
        [[nodiscard]] bool IsSpectatorCameraActive() const noexcept { return m_useSpectatorCamera; }
        [[nodiscard]] const Maths::Vec3D& GetCameraPosition() const noexcept { return m_cameraPosition; }
        [[nodiscard]] const Maths::Vec3D& GetInitialCameraPosition() const noexcept { return m_initialCameraPosition; }
        [[nodiscard]] const Maths::Vec3D& GetCameraTarget() const noexcept { return m_cameraTarget; }
        [[nodiscard]] double GetCameraYaw() const noexcept { return m_cameraYaw; }
        [[nodiscard]] double GetCameraPitch() const noexcept { return m_cameraPitch; }

        // Spatial grid and debug visualization inspection accessors
        [[nodiscard]] Engine::SpatialGrid& GetSpatialGrid() noexcept { return m_spatialGrid; }
        [[nodiscard]] const Engine::SpatialGrid& GetSpatialGrid() const noexcept { return m_spatialGrid; }
        [[nodiscard]] bool IsDebugCellsVisible() const noexcept { return m_showDebugCells; }

        // Landscape terrain inspection accessors
        [[nodiscard]] std::span<const std::shared_ptr<Engine::Terrain::TerrainObject>> GetTerrains() const noexcept { return m_scene.GetTerrains(); }
        [[nodiscard]] size_t GetTerrainCount() const noexcept { return m_scene.GetTerrainCount(); }
        [[nodiscard]] std::shared_ptr<Engine::Terrain::TerrainObject> FindTerrain(std::string_view name) const noexcept { return m_scene.FindTerrain(name); }
        [[nodiscard]] std::shared_ptr<Engine::Terrain::TerrainObject> GetTerrain() const noexcept { return m_scene.GetTerrain(); }

    private:
        static constexpr double MinPitch = -Maths::DegToRad<double> * 89.9;
        static constexpr double MaxPitch =  Maths::DegToRad<double> * 60.0;
        static constexpr double PlayerWalkSpeed = 5.0;

        // Ephemeris and atmosphere mutators
        void SetSunAzimuth(float azimuthRadians) noexcept;
        void SetSunElevation(float elevationRadians) noexcept;
        void SetSunAngles(float azimuthRadians, float elevationRadians) noexcept;
        void UpdateSunDirection() noexcept;
        void SetSolarCycleEnabled(bool enabled) noexcept { m_enableSolarCycle = enabled; }
        void ToggleSolarCycle() noexcept { m_enableSolarCycle = !m_enableSolarCycle; }
        void SetSolarTimeScale(float timeScale) noexcept { m_solarTimeScale = timeScale; }
        void SetAtmosphereParameters(const Engine::AtmosphereParameters& params) noexcept;

        // Camera positioning and mode toggles
        void SetSpectatorCameraActive(bool active) noexcept;
        void ToggleCameraMode() noexcept;
        void SetCameraPosition(const Maths::Vec3D& position);
        void SetCameraPosition(const Maths::Vec3D& position, double yawRadians, double pitchRadians);
        void UpdateCameraVectors();

        // Spatial partitioning and physics resolution
        void RebuildSpatialGrid(const Engine::Terrain::TerrainConfig& config, double minY, double maxY);
        void ResolveCollisions(float deltaTime) noexcept;
        void ResolveBodyTerrainCollision(Engine::Body& body, const Engine::Terrain::TerrainCollider& terrainCollider) noexcept;
        void SetShowDebugCells(bool show) noexcept { m_showDebugCells = show; }
        void ToggleDebugCells() noexcept { m_showDebugCells = !m_showDebugCells; }

    private:
        Renderer::Renderer&                m_renderer;
        Renderer::Renderer*                m_playRenderer{ nullptr };
        Engine::Scene                      m_scene;

        float                              m_sunAzimuth{ 1.096f };
        float                              m_sunElevation{ 1.1664f };
        bool                               m_enableSolarCycle{ false };
        float                              m_solarTimeScale{ 0.05f };
        Engine::AtmosphereParameters       m_atmosphereParams{};

        mutable std::vector<Renderer::RenderItem> m_cachedRenderItems;
        Engine::SpatialGrid                m_spatialGrid{ 1000.0 };
        std::vector<Engine::SpatialCell*>  m_visibleCells;
        std::shared_ptr<Renderer::Mesh>    m_debugCellMesh;
        std::shared_ptr<Renderer::Mesh>    m_activeCellMesh;
        std::shared_ptr<Renderer::Material> m_debugCellMaterial;
        bool                               m_showDebugCells{ false };

        Maths::Vec3D                       m_initialCameraPosition{ -1696.5, 1230.1, -2020.1 };
        Maths::Vec3D                       m_cameraPosition{ -1696.5, 1230.1, -2020.1 };
        Maths::Vec3D                       m_cameraTarget{ 0.0, 0.0, 0.0 };
        double                             m_cameraYaw{ Maths::DegToRad<double> * 40.0 };
        double                             m_cameraPitch{ -Maths::DegToRad<double> * 25.0 };
        bool                               m_useSpectatorCamera{ true };
        bool                               m_wasCameraToggleKeyDown{ false };
        bool                               m_wasOverlayToggleKeyDown{ false };
        bool                               m_wasDebugCellToggleKeyDown{ false };
        POINT                              m_lastMousePos{ 0, 0 };
        bool                               m_hasLastMousePos{ false };
        uint64_t                           m_currentTick{ 0 };
        double                             m_simulationTime{ 0.0 };
    };
}
