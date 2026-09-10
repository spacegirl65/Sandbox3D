// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Application.h"

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

        m_isInitialised = true;
    }

    Application::~Application()
    {
        if (m_isInitialised)
        {
            m_renderer.Shutdown(m_graphicsEngine.GetCommandQueue());
            m_graphicsEngine.Shutdown();
            m_isInitialised = false;
        }
    }

    int Application::Run()
    {
        std::wcout << L"[Application] Entering main render loop...\n";

        while (m_window->ProcessMessages())
        {
            if (!m_window->IsMinimized())
            {
                m_renderer.Render(m_graphicsEngine.GetCommandQueue());
            }
        }

        std::wcout << L"[Application] Exited main render loop cleanly.\n";
        return 0;
    }
}

