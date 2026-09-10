// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Application.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace Sandbox3D::Core
{
    using namespace Sandbox3D::Maths;

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

        // 4. Create solid blue 3D cube mesh
        auto cubeMesh = Renderer::Mesh::CreateCube(m_graphicsEngine.GetDevice(), 1.0f, Vec4::Blue());
        m_renderer.AddRenderItem(std::move(cubeMesh), Mat4x4D::Identity(), "BlueCube");

        // 5. Initialise camera at (5, 5, 5) looking at centre of cube
        UpdateCameraFromOrbit();

        // 6. Hook resize event
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

    void Application::UpdateCameraFromOrbit()
    {
        const double cosEle = std::cos(m_cameraElevation);
        const Vec3D cameraPosition(
            m_cameraDistance * cosEle * std::cos(m_cameraAzimuth),
            m_cameraDistance * std::sin(m_cameraElevation),
            m_cameraDistance * cosEle * std::sin(m_cameraAzimuth)
        );
        const Vec3D cameraTarget(0.0, 0.0, 0.0);

        // Vector pointing from eye to target
        const Vec3D viewDirection = (cameraTarget - cameraPosition).Normalised();

        // Calculate up-vector strictly perpendicular to the view direction
        const Vec3D cameraRight = Vec3D::Up().Cross(viewDirection).Normalised();
        const Vec3D cameraUp    = viewDirection.Cross(cameraRight).Normalised();

        m_renderer.GetCamera().SetLookAt(cameraPosition, cameraTarget, cameraUp);
    }

    int Application::Run()
    {
        std::wcout << L"[Application] Entering main render loop...\n";
        std::wcout << L"[Controls] Arrow keys: Orbit camera | Space: Auto-orbit | R: Reset view\n";

        while (m_window->ProcessMessages())
        {
            if (!m_window->IsMinimized())
            {
                bool cameraMoved = false;

                // Space bar toggles auto-orbit
                const bool spaceIsDown = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
                if (spaceIsDown && !m_spaceWasPressed)
                {
                    m_autoOrbit = !m_autoOrbit;
                }
                m_spaceWasPressed = spaceIsDown;

                if (m_autoOrbit)
                {
                    m_cameraAzimuth += 0.01;
                    cameraMoved = true;
                }

                if (GetAsyncKeyState(VK_LEFT) & 0x8000)
                {
                    m_cameraAzimuth -= 0.025;
                    cameraMoved = true;
                }
                if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
                {
                    m_cameraAzimuth += 0.025;
                    cameraMoved = true;
                }
                if (GetAsyncKeyState(VK_UP) & 0x8000)
                {
                    m_cameraElevation = std::clamp(m_cameraElevation + 0.025, -1.45, 1.45);
                    cameraMoved = true;
                }
                if (GetAsyncKeyState(VK_DOWN) & 0x8000)
                {
                    m_cameraElevation = std::clamp(m_cameraElevation - 0.025, -1.45, 1.45);
                    cameraMoved = true;
                }
                if (GetAsyncKeyState('R') & 0x8000)
                {
                    m_cameraAzimuth   = 0.7853981633974483;
                    m_cameraElevation = 0.6154797086703875;
                    m_autoOrbit       = false;
                    cameraMoved       = true;
                }

                if (cameraMoved)
                {
                    UpdateCameraFromOrbit();
                }

                m_renderer.Render(m_graphicsEngine.GetCommandQueue());
            }
        }

        std::wcout << L"[Application] Exited main render loop cleanly.\n";
        return 0;
    }
}

