// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Base.h"

#include <algorithm>
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

    void Base::Update(const UpdateContext& context)
    {
        Update(context.deltaTime);
        Tick(context.deltaTime);

        if (context.EveryOtherTick())
        {
            TickSemiFixed(context.deltaTime * 2.0f);
        }

        if (context.Every10Ticks())
        {
            TickSlow(context.deltaTime * 10.0f);
        }
    }

    void Base::Update([[maybe_unused]] float deltaTime)
    {
    }

    void Base::Tick([[maybe_unused]] float deltaTime)
    {
    }

    void Base::TickSemiFixed([[maybe_unused]] float deltaTime)
    {
    }

    void Base::TickSlow([[maybe_unused]] float deltaTime)
    {
    }

    void Base::SetPosition(const Maths::Vec3D& position)
    {
        m_worldMatrix.SetTranslation(position);
    }

    void Base::SetWorldMatrix(const Maths::Mat4x4D& worldMatrix)
    {
        m_worldMatrix = worldMatrix;
    }

    void Base::SetParent(const std::shared_ptr<Base>& child, const std::shared_ptr<Base>& parent)
    {
        if (!child || child == parent)
        {
            return;
        }

        // Prevent cycles by rejecting descendants of child as its parent
        for (auto ancestor = parent; ancestor; ancestor = ancestor->GetParent())
        {
            if (ancestor == child)
            {
                return;
            }
        }

        auto currentParent = child->m_parent.lock();
        if (currentParent == parent)
        {
            return;
        }

        if (currentParent)
        {
            std::erase(currentParent->m_children, child);
        }

        child->m_parent = parent;
        if (parent)
        {
            if (std::find(parent->m_children.begin(), parent->m_children.end(), child) == parent->m_children.end())
            {
                parent->m_children.push_back(child);
            }
        }
    }
}

