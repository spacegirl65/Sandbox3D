// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include <cstdint>

namespace Sandbox3D::Core
{
    // High-precision global simulation timing, frame pacing, and delta time tracking utility
    class Time final
    {
    public:
        // Simulation delta time in seconds (Time.DeltaTime)
        [[nodiscard]] static float GetDeltaTime() noexcept { return s_deltaTime; }
        [[nodiscard]] static double GetDeltaTimeDouble() noexcept { return static_cast<double>(s_deltaTime); }
        [[nodiscard]] static float DeltaTime() noexcept { return s_deltaTime; }

        // Unscaled frame pacing delta time in seconds
        [[nodiscard]] static float GetUnscaledDeltaTime() noexcept { return s_unscaledDeltaTime; }
        [[nodiscard]] static double GetUnscaledDeltaTimeDouble() noexcept { return static_cast<double>(s_unscaledDeltaTime); }
        [[nodiscard]] static float UnscaledDeltaTime() noexcept { return s_unscaledDeltaTime; }

        // Total elapsed simulation time in seconds
        [[nodiscard]] static float GetTotalTime() noexcept { return s_totalTime; }
        [[nodiscard]] static double GetTotalTimeDouble() noexcept { return static_cast<double>(s_totalTime); }
        [[nodiscard]] static float TotalTime() noexcept { return s_totalTime; }

        // Cumulative simulation tick counter
        [[nodiscard]] static uint64_t GetTickCount() noexcept { return s_tickCount; }

        // Updates simulation delta time and total elapsed time
        static void UpdateSimulation(float deltaTime) noexcept
        {
            s_deltaTime = deltaTime;
            s_totalTime += deltaTime;
            ++s_tickCount;
        }

        // Updates unscaled render frame delta time
        static void UpdateFrame(float unscaledDeltaTime) noexcept
        {
            s_unscaledDeltaTime = unscaledDeltaTime;
        }

    private:
        inline static float    s_deltaTime{ 1.0f / 60.0f };
        inline static float    s_unscaledDeltaTime{ 1.0f / 60.0f };
        inline static float    s_totalTime{ 0.0f };
        inline static uint64_t s_tickCount{ 0 };
    };
}

namespace Sandbox3D
{
    using Time = Core::Time;
}

