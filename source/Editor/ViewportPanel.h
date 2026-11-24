// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "EditorContext.h"
#include "Engine/Camera.h"
#include "Renderer/Renderer.h"
#include "Renderer/SwapChain.h"

#include <d3d12.h>
#include <dxgi1_6.h>

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <string>

struct ImGuiContext;

namespace Sandbox3D::Editor
{
    // Renders the offscreen 3D editor viewport, playback toolbar, and free-look camera controls
    class ViewportPanel final
    {
    public:
        using SrvAllocFn = std::function<void(D3D12_CPU_DESCRIPTOR_HANDLE*, D3D12_GPU_DESCRIPTOR_HANDLE*)>;
        using SrvFreeFn  = std::function<void(D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE)>;

        void Initialise(
            IDXGIFactory6* factory,
            ID3D12Device* device,
            ID3D12CommandQueue* commandQueue,
            const std::wstring& gpuDescription,
            const Renderer::Renderer* sourceRenderer,
            const SrvAllocFn& allocateSrv
        );
        void Shutdown(ID3D12CommandQueue* commandQueue, const SrvFreeFn& freeSrv) noexcept;

        void Render(
            ID3D12Device* device,
            ID3D12CommandQueue* commandQueue,
            std::span<const Renderer::RenderItem> renderItems,
            std::span<const Renderer::GpuLight> lights,
            float deltaTime,
            float updatesPerSecond,
            float targetUps,
            bool windowFocused,
            ImGuiContext* imguiContext,
            const SrvAllocFn& allocateSrv
        );

        void Draw();
        void UpdateCamera(float deltaTime, bool windowFocused, ImGuiContext* imguiContext);
        void SetCameraPose(const Maths::Vec3D& position, double yaw, double pitch) noexcept;
        void ClearFocus() noexcept { m_isFocused = false; }
        [[nodiscard]] bool NeedsResize() const noexcept
        {
            return m_desiredWidth > 0 && m_desiredHeight > 0 && (m_desiredWidth != m_width || m_desiredHeight != m_height);
        }

        [[nodiscard]] Engine::Camera& GetCamera() noexcept { return m_camera; }
        [[nodiscard]] Renderer::Renderer& GetRenderer() noexcept { return m_renderer; }
        [[nodiscard]] PlayState GetPlayState() const noexcept { return m_playState; }
        void SetPlayState(PlayState state) noexcept { m_playState = state; }
        void SetPlayStateCallback(PlayStateCallback callback) { m_playStateCallback = std::move(callback); }

    private:
        void UpdateSrv(ID3D12Device* device, const SrvAllocFn& allocateSrv);
        void DrawToolbar();

    private:
        static constexpr uint32_t InitialWidth  = 1280;
        static constexpr uint32_t InitialHeight = 720;

        Renderer::Renderer  m_renderer;
        Engine::Camera      m_camera{ "EditorCamera" };
        std::array<D3D12_CPU_DESCRIPTOR_HANDLE, Renderer::SwapChain::BufferCount> m_srvCpuHandles{};
        std::array<D3D12_GPU_DESCRIPTOR_HANDLE, Renderer::SwapChain::BufferCount> m_srvGpuHandles{};
        bool                m_hasSrv{ false };
        uint32_t            m_width{ InitialWidth };
        uint32_t            m_height{ InitialHeight };
        uint32_t            m_desiredWidth{ 0 };
        uint32_t            m_desiredHeight{ 0 };
        bool                m_isHovered{ false };
        bool                m_isFocused{ false };
        Maths::Vec3D        m_cameraPosition{ 4.9, 10.0, -2660.0 };
        double              m_cameraYaw{ -0.508 };
        double              m_cameraPitch{ -0.22 };
        PlayState           m_playState{ PlayState::Stopped };
        PlayStateCallback   m_playStateCallback;
    };
}
