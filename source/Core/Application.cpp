// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Application.h"
#include "Engine/Sandbox.h"

#include <algorithm>
#include <chrono>
#include <thread>
#include <iostream>
#include <timeapi.h>

#pragma comment(lib, "winmm.lib")

namespace Sandbox3D::Core
{
    Application::Application(uint32_t width, uint32_t height, const std::wstring& title)
    {
        // 1. Initialise Win32 Desktop Window
        m_window = std::make_unique<Window>(width, height, title);

        // 2. Initialise Direct3D 12 Hardware Subsystem Facade
        m_graphicsEngine.Initialise(/* enableDebugLayer = */ true);

        // 3. Initialise Renderer subsystem
        m_renderer.Initialise(
            m_graphicsEngine.GetFactory(),
            m_graphicsEngine.GetDevice(),
            m_graphicsEngine.GetCommandQueue(),
            m_window->GetHwnd(),
            m_window->GetWidth(),
            m_window->GetHeight(),
            m_graphicsEngine.GetGpuDescription()
        );
        m_renderer.SetTargetFps(static_cast<float>(m_targetFps));
        m_renderer.SetTargetUps(static_cast<float>(m_targetUps));

        // 4. Hook resize event
        m_window->SetResizeCallback([this](uint32_t newWidth, uint32_t newHeight)
        {
            m_renderer.OnResize(
                m_graphicsEngine.GetDevice(),
                m_graphicsEngine.GetCommandQueue(),
                newWidth,
                newHeight
            );
        });

        // 5. Initialise Sandbox domain logic
        m_sandbox = std::make_unique<Sandbox>(m_renderer, m_graphicsEngine.GetDevice());

        m_isInitialised = true;

        // Reinforce terminal title after complete subsystem initialisation
        Window::ApplyTerminalTitle();
    }

    Application::~Application()
    {
        if (m_isInitialised)
        {
            m_sandbox.reset();
            m_renderer.Shutdown(m_graphicsEngine.GetCommandQueue());
            m_graphicsEngine.Shutdown();
            m_isInitialised = false;
        }
    }

    void Application::SetTargetFps(double targetFps) noexcept
    {
        m_targetFps = std::max(1.0, targetFps);
        m_renderer.SetTargetFps(static_cast<float>(m_targetFps));
    }

    void Application::SetTargetUps(double targetUps) noexcept
    {
        m_targetUps = std::max(1.0, targetUps);
        m_renderer.SetTargetUps(static_cast<float>(m_targetUps));
    }

    int Application::Run()
    {
        // Enforce terminal title immediately prior to commencing render loop
        Window::ApplyTerminalTitle();

        std::wcout << L"[Application] Entering main render loop...\n";

        // Request 1 ms OS timer resolution for sub-millisecond scheduling precision
        timeBeginPeriod(1);

        auto previousTime = std::chrono::high_resolution_clock::now();
        auto nextFrameTime = previousTime;
        auto lastTitleEnforceTime = previousTime;

        // Fixed simulation timestep accumulator state
        double timeAccumulator = 0.0;
        constexpr double maxFrameDeltaTime = 0.1; // Safety clamp (100 ms) against window pause/drag
        constexpr uint32_t maxSubSteps = 4;      // Spiral of death prevention

        // Telemetry tracking for updates per second (UPS)
        double upsTimeAccumulator = 0.0;
        uint32_t upsCounter = 0;
        float currentUps = static_cast<float>(m_targetUps);

        while (m_window->ProcessMessages())
        {
            const auto frameStartTime = std::chrono::high_resolution_clock::now();

            // Periodically enforce terminal title to prevent terminal host or shell overrides
            if (std::chrono::duration<float>(frameStartTime - lastTitleEnforceTime).count() >= 1.0f)
            {
                lastTitleEnforceTime = frameStartTime;
                Window::ApplyTerminalTitle();
            }

            if (!m_window->IsMinimized())
            {
                const double rawFrameDelta = std::chrono::duration<double>(frameStartTime - previousTime).count();
                previousTime = frameStartTime;

                // Clamp frame delta to protect simulation against severe latency spikes
                const double frameDeltaTime = std::min(rawFrameDelta, maxFrameDeltaTime);
                timeAccumulator += frameDeltaTime;

                // Fixed simulation parameters derived dynamically from m_targetUps
                const double fixedTimeStep = 1.0 / m_targetUps;
                const float fixedDeltaTime = static_cast<float>(fixedTimeStep);

                const bool isFocused = m_window->IsFocused();
                uint32_t subSteps = 0;

                // Fixed-cadence simulation update loop (decoupled from render pacing)
                while (timeAccumulator >= fixedTimeStep && subSteps < maxSubSteps)
                {
                    m_sandbox->Update(fixedDeltaTime, isFocused);
                    timeAccumulator -= fixedTimeStep;
                    ++subSteps;
                    ++upsCounter;
                }

                // If execution backlog exceeds maxSubSteps, discard residual accumulator to prevent spiral of death
                if (timeAccumulator >= fixedTimeStep)
                {
                    timeAccumulator = 0.0;
                }

                // Smooth UPS telemetry over a longer measurement window (2.0s) to eliminate timing jitter
                constexpr double upsAverageWindow = 2.0;
                upsTimeAccumulator += rawFrameDelta;
                if (upsTimeAccumulator >= upsAverageWindow)
                {
                    const double effectiveTicks = static_cast<double>(upsCounter) + (timeAccumulator / fixedTimeStep);
                    const float measuredUps = static_cast<float>(effectiveTicks / upsTimeAccumulator);
                    currentUps = std::min(measuredUps, static_cast<float>(m_targetUps));
                    upsTimeAccumulator = 0.0;
                    upsCounter = 0;
                }

                // Render frame pass
                m_renderer.Render(
                    m_graphicsEngine.GetCommandQueue(),
                    m_sandbox->GetRenderItems(),
                    m_sandbox->GetLightData(),
                    /* vSync = */ false,
                    m_sandbox->GetObjects().size(),
                    currentUps,
                    static_cast<float>(m_targetUps)
                );

                // High-precision frame presentation pacing dynamically targetting m_targetFps
                const auto targetFrameDuration = std::chrono::ceil<std::chrono::high_resolution_clock::duration>(
                    std::chrono::duration<double>(1.0 / m_targetFps)
                );
                nextFrameTime += targetFrameDuration;

                const auto workEndTime = std::chrono::high_resolution_clock::now();
                if (workEndTime < nextFrameTime)
                {
                    const auto remaining = nextFrameTime - workEndTime;

                    // Sleep for coarse remainder minus 1.5 ms to avoid OS scheduler oversleep
                    if (remaining > std::chrono::microseconds(2000))
                    {
                        std::this_thread::sleep_for(remaining - std::chrono::microseconds(1500));
                    }

                    // High-precision spin-wait for final sub-millisecond increment
                    while (std::chrono::high_resolution_clock::now() < nextFrameTime)
                    {
                        YieldProcessor();
                    }
                }
                else
                {
                    // If frame execution exceeded the target duration, resynchronise cadence to prevent catch-up bursts
                    nextFrameTime = workEndTime;
                }
            }
            else
            {
                previousTime = frameStartTime;
                nextFrameTime = frameStartTime;
                timeAccumulator = 0.0;
                upsCounter = 0;
                upsTimeAccumulator = 0.0;
                currentUps = static_cast<float>(m_targetUps);
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
        }

        timeEndPeriod(1);

        std::wcout << L"[Application] Exited main render loop cleanly.\n";
        return 0;
    }
}

