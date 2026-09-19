// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Maths/Maths.h"
#include "Renderer/RenderItem.h"
#include "UpdateContext.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace Sandbox3D::Engine
{
    // Abstract base class representing any scene entity/object in the simulation hierarchy
    class Base
    {
    public:
        explicit Base(std::string_view name = "Base");
        virtual ~Base() = default;

        Base(const Base&) = delete;
        Base& operator=(const Base&) = delete;
        Base(Base&&) noexcept = default;
        Base& operator=(Base&&) noexcept = default;

        // Core polymorphic update loop contracts
        virtual void Update(const UpdateContext& context);
        virtual void Update(float deltaTime);

        // Tiered simulation update hooks
        virtual void Tick(float deltaTime);
        virtual void TickSemiFixed(float deltaTime);
        virtual void TickSlow(float deltaTime);

        // Identification & metadata
        [[nodiscard]] uint32_t GetId() const noexcept { return m_id; }
        [[nodiscard]] const std::string& GetName() const noexcept { return m_name; }
        void SetName(std::string_view name) { m_name = name; }

        // Simulation lifecycle & enablement
        [[nodiscard]] bool IsActive() const noexcept { return m_isActive; }
        void SetActive(bool active) noexcept { m_isActive = active; }

        // 64-bit spatial transform state (Rules 18 & 19 - dual-tier coordinate infrastructure)
        [[nodiscard]] virtual const Maths::Vec3D& GetPosition() const noexcept { return m_position; }
        virtual void SetPosition(const Maths::Vec3D& position);

        [[nodiscard]] virtual const Maths::Mat4x4D& GetWorldMatrix() const noexcept { return m_worldMatrix; }
        virtual void SetWorldMatrix(const Maths::Mat4x4D& worldMatrix);

        // Renderable query interface
        [[nodiscard]] virtual bool IsRenderable() const noexcept { return false; }
        [[nodiscard]] virtual bool IsVisible() const noexcept { return m_isVisible; }
        virtual void SetVisible(bool visible) noexcept { m_isVisible = visible; }
        [[nodiscard]] virtual std::span<const Renderer::RenderItem> GetRenderItems() const noexcept { return {}; }

    protected:
        uint32_t        m_id{ GenerateNextId() };
        std::string     m_name;
        bool            m_isActive{ true };
        bool            m_isVisible{ true };
        Maths::Vec3D    m_position{ 0.0, 0.0, 0.0 };
        Maths::Mat4x4D  m_worldMatrix{ Maths::Mat4x4D::Identity() };

    private:
        static uint32_t GenerateNextId() noexcept;
    };
}

