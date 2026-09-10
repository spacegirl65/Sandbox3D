// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Application.h"

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

        const Mat4x4D cameraOrigin = Mat4x4D::World(Vec3D(5, 5, 5));
        const Vec3D cameraPosition = cameraOrigin.GetTranslation();
        const Vec3D cameraTarget(0, 0, 0);

        // Vector pointing from eye to target
        const Vec3D viewDirection = (cameraTarget - cameraPosition).Normalised();

        // Calculate up-vector strictly perpendicular to the view direction
        const Vec3D cameraRight = Vec3D::Up().Cross(viewDirection).Normalised();
        const Vec3D cameraUp = viewDirection.Cross(cameraRight).Normalised();

        m_renderer.GetCamera().SetLookAt(cameraPosition, cameraTarget, cameraUp);

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

