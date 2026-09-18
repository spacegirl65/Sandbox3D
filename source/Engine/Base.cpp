// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Base.h"

#include <atomic>

namespace Sandbox3D::Engine
{
    Base::Base(std::string_view name)
        : m_name(name)
    {
    }

    uint32_t Base::GenerateNextId() noexcept
    {
        static std::atomic<uint32_t> s_nextId{ 1 };
        return s_nextId.fetch_add(1, std::memory_order_relaxed);
    }

    void Base::SetPosition(const Maths::Vec3D& position)
    {
        m_position = position;
        m_worldMatrix = Maths::Mat4x4D::Translation(position.x, position.y, position.z);
    }

    void Base::SetWorldMatrix(const Maths::Mat4x4D& worldMatrix)
    {
        m_worldMatrix = worldMatrix;
        m_position    = worldMatrix.GetTranslation();
    }
}

