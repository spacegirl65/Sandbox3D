// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Application.h"
#include "../Sandbox.h"

#include <chrono>
#include <thread>
#include <iostream>

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
            m_window->GetHeight()
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
        std::wcout << L"[Controls] Arrow keys: Orbit camera | PgUp/PgDn: Zoom | Space: Auto-orbit | R: Reset view\n";

        auto previousTime = std::chrono::high_resolution_clock::now();
        auto lastTitleEnforceTime = previousTime;

        while (m_window->ProcessMessages())
        {
            const auto currentTime = std::chrono::high_resolution_clock::now();

            // Periodically enforce terminal title to prevent terminal host or shell overrides
            if (std::chrono::duration<float>(currentTime - lastTitleEnforceTime).count() >= 1.0f)
            {
                lastTitleEnforceTime = currentTime;
                Window::ApplyTerminalTitle();
            }

            if (!m_window->IsMinimized())
            {
                const float deltaTime = std::chrono::duration<float>(currentTime - previousTime).count();
                previousTime = currentTime;

                const bool isFocused = m_window->IsFocused();
                m_sandbox->Update(deltaTime, isFocused);
                m_renderer.Render(m_graphicsEngine.GetCommandQueue(), m_sandbox->GetRenderItems());
            }
            else
            {
                previousTime = currentTime;
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
        }

        std::wcout << L"[Application] Exited main render loop cleanly.\n";
        return 0;
    }
}

