// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include <cstdint>

namespace Sandbox3D::Engine
{
    // Encapsulates simulation timing, tick indexing, and phased execution queries for fixed-timestep updates
    struct UpdateContext
    {
        float    deltaTime{ 1.0f / 60.0f };
        uint64_t tickIndex{ 0 };
        float    totalTime{ 0.0f };

        // Evaluates whether the current simulation tick coincides with an arbitrary N-tick cadence
        [[nodiscard]] constexpr bool EveryNTicks(uint32_t n) const noexcept
        {
            return n > 0 && (tickIndex % n == 0);
        }

        // Evaluates whether the current simulation tick executes on alternate cycles (30 Hz at 60 Hz base)
        [[nodiscard]] constexpr bool EveryOtherTick() const noexcept
        {
            return (tickIndex & 1) == 0;
        }

        // Evaluates whether the current simulation tick executes on decenary cycles (6 Hz at 60 Hz base)
        [[nodiscard]] constexpr bool Every10Ticks() const noexcept
        {
            return (tickIndex % 10) == 0;
        }
    };
}

