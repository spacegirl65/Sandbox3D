// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Renderer/Renderer.h"
#include "Renderer/RenderItem.h"

#include <d3d12.h>
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

        void Update(float deltaTime);

        // Render object management
        void AddRenderItem(Renderer::RenderItem item);
        void AddRenderItem(std::shared_ptr<Renderer::Mesh> mesh, const Maths::Mat4x4D& worldMatrix = Maths::Mat4x4D::Identity(), const std::string& name = {});
        void RemoveRenderItem(std::string_view name);
        void ClearRenderItems() noexcept;

        [[nodiscard]] std::span<const Renderer::RenderItem> GetRenderItems() const noexcept { return m_renderItems; }
        [[nodiscard]] std::vector<Renderer::RenderItem>& GetRenderItems() noexcept { return m_renderItems; }
        [[nodiscard]] Renderer::RenderItem* FindRenderItem(std::string_view name) noexcept;
        [[nodiscard]] const Renderer::RenderItem* FindRenderItem(std::string_view name) const noexcept;

        [[nodiscard]] bool IsAutoOrbiting() const noexcept { return m_autoOrbit; }
        void SetAutoOrbit(bool autoOrbit) noexcept { m_autoOrbit = autoOrbit; }

        // Camera positioning
        void SetCameraPosition(const Maths::Vec3D& position);
        [[nodiscard]] const Maths::Vec3D& GetInitialCameraPosition() const noexcept { return m_initialCameraPosition; }

    private:
        void UpdateCameraFromOrbit();

    private:
        Renderer::Renderer&                 m_renderer;
        std::vector<Renderer::RenderItem>   m_renderItems;
        Maths::Vec3D                        m_initialCameraPosition{ 0.0, 5.0, 5.0 };
        double                              m_cameraDistance{ 0.0 };
        double                              m_cameraAzimuth{ 0.0 };
        double                              m_cameraElevation{ 0.0 };
        bool                                m_autoOrbit{ false };
        bool                                m_spaceWasPressed{ false };
    };
}

