// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Application.h"
#include "Engine/Sandbox.h"

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

    int Application::Run()
    {
        // Enforce terminal title immediately prior to commencing render loop
        Window::ApplyTerminalTitle();

        std::wcout << L"[Application] Entering main render loop...\n";

        // Target update loop frequency: 120 FPS
        constexpr double targetFps = 120.0;
        constexpr auto targetFrameDuration = std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(
            std::chrono::duration<double>(1.0 / targetFps)
        );

        // Request 1 ms OS timer resolution for sub-millisecond scheduling precision
        timeBeginPeriod(1);

        auto previousTime = std::chrono::high_resolution_clock::now();
        auto lastTitleEnforceTime = previousTime;

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
                const float deltaTime = std::chrono::duration<float>(frameStartTime - previousTime).count();
                previousTime = frameStartTime;

                const bool isFocused = m_window->IsFocused();
                m_sandbox->Update(deltaTime, isFocused);
                m_renderer.Render(m_graphicsEngine.GetCommandQueue(), m_sandbox->GetRenderItems());

                // Limit update loop to target frame duration (120 FPS)
                const auto workEndTime = std::chrono::high_resolution_clock::now();
                const auto elapsed = workEndTime - frameStartTime;
                if (elapsed < targetFrameDuration)
                {
                    const auto remaining = targetFrameDuration - elapsed;

                    // Sleep for coarse remainder minus 1.5 ms to avoid OS scheduler oversleep
                    if (remaining > std::chrono::microseconds(2000))
                    {
                        std::this_thread::sleep_for(remaining - std::chrono::microseconds(1500));
                    }

                    // High-precision spin-wait for final sub-millisecond increment
                    while (std::chrono::high_resolution_clock::now() - frameStartTime < targetFrameDuration)
                    {
                        YieldProcessor();
                    }
                }
            }
            else
            {
                previousTime = frameStartTime;
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
        }

        timeEndPeriod(1);

        std::wcout << L"[Application] Exited main render loop cleanly.\n";
        return 0;
    }
}

