// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Window.h"
#include "Renderer/GraphicsEngine.h"
#include "Renderer/Renderer.h"

#include <memory>
#include <cstdint>

namespace Sandbox3D
{
    class Sandbox;
}

namespace Sandbox3D::Core
{
    // Application controller managing system lifecycle, windowing, and the primary rendering loop
    class Application final
    {
    public:
        Application(uint32_t width = 2880, uint32_t height = 1200, const std::wstring& title = {});
        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;
        Application(Application&&) noexcept = delete;
        Application& operator=(Application&&) noexcept = delete;

        int Run();

        // Cadence configuration (Rules 23, 26)
        void SetTargetFps(double targetFps) noexcept;
        [[nodiscard]] double GetTargetFps() const noexcept { return m_targetFps; }

        void SetTargetUps(double targetUps) noexcept;
        [[nodiscard]] double GetTargetUps() const noexcept { return m_targetUps; }

    private:
        std::unique_ptr<Window>       m_window;
        GraphicsEngine                m_graphicsEngine;
        Renderer::Renderer            m_renderer;
        std::unique_ptr<Sandbox>      m_sandbox;
        double                        m_targetFps{ 120.0 };
        double                        m_targetUps{ 60.0 };
        bool                          m_isInitialised{ false };
    };
}

