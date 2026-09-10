// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Window.h"
#include "../GraphicsEngine.h"
#include "../Renderer/Renderer.h"

#include <memory>
#include <cstdint>

namespace Sandbox3D::Core
{
    // Application controller managing system lifecycle, windowing, and the primary rendering loop
    class Application final
    {
    public:
        Application(uint32_t width = 2880, uint32_t height = 1200, const std::wstring& title = L"Sandbox3D - [DX12, 2880 x 1200]");
        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;
        Application(Application&&) noexcept = delete;
        Application& operator=(Application&&) noexcept = delete;

        int Run();

    private:
        void UpdateCameraFromOrbit();

    private:
        std::unique_ptr<Window>  m_window;
        GraphicsEngine           m_graphicsEngine;
        Renderer::Renderer       m_renderer;
        double                   m_cameraDistance{ 8.660254037844386 }; // sqrt(75.0)
        double                   m_cameraAzimuth{ 0.7853981633974483 };  // pi / 4
        double                   m_cameraElevation{ 0.6154797086703875 }; // asin(1 / sqrt(3))
        bool                     m_autoOrbit{ false };
        bool                     m_spaceWasPressed{ false };
        bool                     m_isInitialised{ false };
    };
}

