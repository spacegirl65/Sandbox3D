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
        Application(uint32_t width = 1280, uint32_t height = 720, const std::wstring& title = L"Sandbox3D - DirectX 12 Triangle");
        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;
        Application(Application&&) noexcept = delete;
        Application& operator=(Application&&) noexcept = delete;

        int Run();

    private:
        std::unique_ptr<Window>  m_window;
        GraphicsEngine           m_graphicsEngine;
        Renderer::Renderer       m_renderer;
        bool                     m_isInitialised{ false };
    };
}

