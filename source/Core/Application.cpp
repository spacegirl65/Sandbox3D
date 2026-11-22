// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Application.h"
#include "Logger.h"
#include "Time.h"
#include "Editor/EditorLogger.h"
#include "Editor/EditorWindow.h"
#include "Engine/Sandbox.h"

#include <algorithm>
#include <chrono>
#include <thread>
#include <timeapi.h>

#pragma comment(lib, "winmm.lib")

namespace Sandbox3D::Core
{
    Application::Application(uint32_t width, uint32_t height, const std::wstring& title)
        : m_playWindowWidth(width)
        , m_playWindowHeight(height)
        , m_playWindowTitle(title)
    {
        Logger::RegisterSink(&m_consoleLogger);

        // Initialise Direct3D 12 Hardware Subsystem Facade
        m_graphicsEngine.Initialise(/* enableDebugLayer = */ true);

        // Initialise Editor window first as the sole startup window
        m_editorWindow = std::make_unique<Editor::EditorWindow>();
        m_editorWindow->Initialise(
            m_graphicsEngine.GetFactory(),
            m_graphicsEngine.GetDevice(),
            m_graphicsEngine.GetCommandQueue(),
            m_graphicsEngine.GetGpuDescription()
        );

        m_editorLogger = std::make_unique<Editor::EditorLogger>(m_editorWindow.get());
        Logger::RegisterSink(m_editorLogger.get());

        Core::Logger::LogInfo("[Editor] Sandbox3D window created.");

        // Initialise Sandbox domain logic bound to the Editor's offscreen viewport renderer
        m_sandbox = std::make_unique<Sandbox>(
            m_editorWindow->GetViewportRenderer(),
            m_graphicsEngine.GetDevice(),
            m_graphicsEngine.GetCommandQueue()
        );

        // Preserve the Editor's independent camera in the Editor Viewport and sync its initial pose
        m_editorWindow->SetScene(&m_sandbox->GetScene());
        m_editorWindow->GetViewportRenderer().SetCamera(&m_editorWindow->GetEditorCamera());
        m_editorWindow->SetCameraPose(
            m_sandbox->GetCameraPosition(),
            m_sandbox->GetCameraYaw(),
            m_sandbox->GetCameraPitch()
        );

        m_editorWindow->SetPlayStateCallback([this](Editor::PlayState newState)
        {
            if (newState == Editor::PlayState::Playing)
            {
                if (!m_window)
                {
                    StartPlayWindow();
                }
            }
            else if (newState == Editor::PlayState::Stopped)
            {
                StopPlayWindow();
            }
        });

        m_isInitialised = true;

        // Reinforce terminal title after complete subsystem initialisation
        Window::ApplyTerminalTitle();
    }

    Application::~Application()
    {
        if (m_isInitialised)
        {
            StopPlayWindow();
            if (m_editorLogger)
            {
                Logger::UnregisterSink(m_editorLogger.get());
                m_editorLogger.reset();
            }
            if (m_editorWindow)
            {
                m_editorWindow->Shutdown(m_graphicsEngine.GetCommandQueue());
                m_editorWindow.reset();
            }
            m_sandbox.reset();
            m_graphicsEngine.Shutdown();
            Logger::UnregisterSink(&m_consoleLogger);
            m_isInitialised = false;
        }
    }

    void Application::StartPlayWindow()
    {
        if (m_window)
        {
            return;
        }

        WindowOptions playOptions{};
        playOptions.quitOnClose = false;

        m_window = std::make_unique<Window>(m_playWindowWidth, m_playWindowHeight, m_playWindowTitle, playOptions);

        m_renderer.Initialise(
            m_graphicsEngine.GetFactory(),
            m_graphicsEngine.GetDevice(),
            m_graphicsEngine.GetCommandQueue(),
            m_window->GetHwnd(),
            m_window->GetWidth(),
            m_window->GetHeight(),
            m_graphicsEngine.GetGpuDescription()
        );
        m_renderer.CopyEnvironmentSettingsFrom(m_editorWindow->GetViewportRenderer());
        m_renderer.SetTargetFps(static_cast<float>(m_targetFps));
        m_renderer.SetTargetUps(static_cast<float>(m_targetUps));
        m_renderer.SetShowGizmo(false);
        m_renderer.SetCamera(m_sandbox->GetActiveCamera());
        m_sandbox->SetPlayRenderer(&m_renderer);

        m_window->SetResizeCallback([this](uint32_t newWidth, uint32_t newHeight)
        {
            m_renderer.OnResize(
                m_graphicsEngine.GetDevice(),
                m_graphicsEngine.GetCommandQueue(),
                newWidth,
                newHeight
            );
        });
    }

    void Application::StopPlayWindow() noexcept
    {
        if (!m_window)
        {
            return;
        }

        if (m_sandbox)
        {
            m_sandbox->SetPlayRenderer(nullptr);
        }
        m_renderer.Shutdown(m_graphicsEngine.GetCommandQueue());
        m_window.reset();
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

        Logger::LogInfo("[Application] Entering main render loop.");

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

        while (m_editorWindow && m_editorWindow->GetWindow()->ProcessMessages() && m_editorWindow->IsRunning())
        {
            if (m_window && !m_window->IsRunning())
            {
                StopPlayWindow();
                m_editorWindow->SetPlayState(Editor::PlayState::Stopped);
                Logger::LogInfo("[Editor] Simulation loop stopped.");
            }

            const auto frameStartTime = std::chrono::high_resolution_clock::now();

            // Periodically enforce terminal title to prevent terminal host or shell overrides
            if (std::chrono::duration<float>(frameStartTime - lastTitleEnforceTime).count() >= 1.0f)
            {
                lastTitleEnforceTime = frameStartTime;
                Window::ApplyTerminalTitle();
            }

            const bool isRendererActive = m_window && m_window->IsRunning() && !m_window->IsMinimized();
            const bool isEditorActive   = m_editorWindow && !m_editorWindow->IsMinimized();
            const bool isAnyWindowFocused = (m_window && m_window->IsFocused()) || (m_editorWindow && m_editorWindow->IsFocused());

            // Run simulation and rendering while at least one window is visible and active
            if ((isRendererActive || isEditorActive) && isAnyWindowFocused)
            {
                const double rawFrameDelta = std::chrono::duration<double>(frameStartTime - previousTime).count();
                previousTime = frameStartTime;

                // Clamp frame delta to protect simulation against severe latency spikes
                const double frameDeltaTime = std::min(rawFrameDelta, maxFrameDeltaTime);
                timeAccumulator += frameDeltaTime;

                Time::UpdateFrame(static_cast<float>(frameDeltaTime));

                // Fixed simulation parameters derived dynamically from m_targetUps
                const double fixedTimeStep = 1.0 / m_targetUps;
                const float fixedDeltaTime = static_cast<float>(fixedTimeStep);

                const bool isPlayWindowFocused = m_window && m_window->IsFocused();
                const bool isPlaying = (m_editorWindow->GetPlayState() == Editor::PlayState::Playing);
                uint32_t subSteps = 0;

                // Fixed-cadence simulation update loop (decoupled from render pacing)
                if (isPlaying)
                {
                    while (timeAccumulator >= fixedTimeStep && subSteps < maxSubSteps)
                    {
                        Time::UpdateSimulation(fixedDeltaTime);
                        m_sandbox->Update(fixedDeltaTime, isPlayWindowFocused);
                        timeAccumulator -= fixedTimeStep;
                        ++subSteps;
                        ++upsCounter;
                    }
                }
                else
                {
                    timeAccumulator = 0.0;
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

                // Render Play DX12 window only while actively playing (suspended on Pause)
                const bool shouldRenderPlayWindow = isRendererActive && isPlaying;
                if (shouldRenderPlayWindow)
                {
                    m_renderer.Render(
                        m_graphicsEngine.GetCommandQueue(),
                        m_sandbox->GetRenderItems(),
                        m_sandbox->GetLightData(),
                        /* vSync = */ m_vSync,
                        m_sandbox->GetObjects().size(),
                        currentUps,
                        static_cast<float>(m_targetUps)
                    );
                }

                // Render Editor offscreen 3D viewport and Editor UI window
                if (isEditorActive)
                {
                    m_editorWindow->RenderViewport(
                        m_graphicsEngine.GetDevice(),
                        m_graphicsEngine.GetCommandQueue(),
                        m_sandbox->GetRenderItems(/* includeDebugOverlays = */ false),
                        m_sandbox->GetLightData(),
                        static_cast<float>(frameDeltaTime),
                        currentUps,
                        static_cast<float>(m_targetUps)
                    );

                    const bool editorVSync = shouldRenderPlayWindow ? false : m_vSync;
                    m_editorWindow->Render(m_graphicsEngine.GetCommandQueue(), editorVSync);
                }

                // When V-Sync is disabled, enforce software presentation pacing targeting m_targetFps
                if (!m_vSync)
                {
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

        Logger::LogInfo("[Application] Exited main render loop cleanly.");
        return 0;
    }
}

