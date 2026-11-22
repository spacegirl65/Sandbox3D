// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "Core/Window.h"
#include "Renderer/CommandContext.h"
#include "Renderer/DxCheck.h"
#include "Renderer/SwapChain.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct ImFont;
struct ImGuiContext;

namespace Sandbox3D::Editor
{
    using Microsoft::WRL::ComPtr;

    // Hosts the standalone Editor desktop window, its dedicated D3D12 swap chain, and the Dear ImGui workspace.
    class EditorWindow final
    {
    public:
        EditorWindow() = default;
        ~EditorWindow();

        EditorWindow(const EditorWindow&) = delete;
        EditorWindow& operator=(const EditorWindow&) = delete;
        EditorWindow(EditorWindow&&) noexcept = delete;
        EditorWindow& operator=(EditorWindow&&) noexcept = delete;

        void Initialise(
            IDXGIFactory6* factory,
            ID3D12Device* device,
            ID3D12CommandQueue* commandQueue
        );

        void Shutdown(ID3D12CommandQueue* commandQueue) noexcept;

        void Render(ID3D12CommandQueue* commandQueue, bool vSync = true);

        void AppendTerminalLog(std::string entry);

        [[nodiscard]] bool IsRunning() const noexcept;
        [[nodiscard]] bool IsMinimized() const noexcept;
        [[nodiscard]] bool IsFocused() const noexcept;
        [[nodiscard]] Core::Window* GetWindow() const noexcept { return m_window.get(); }

        void AllocateSrvDescriptor(
            D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle,
            D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle
        );
        void FreeSrvDescriptor(
            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
            D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle
        ) noexcept;

    private:
        void CreateSrvDescriptorHeap(ID3D12Device* device);
        void EnableDarkTitleBar() noexcept;
        void LoadFonts(float dpiScale);
        void InitialiseImGui(ID3D12Device* device, ID3D12CommandQueue* commandQueue);
        void ConfigureStyle(float dpiScale);
        void OnResize(ID3D12Device* device, ID3D12CommandQueue* commandQueue, uint32_t width, uint32_t height);

        void BuildWorkspace();
        void DrawMainMenuBar();
        void BuildDefaultDockLayout(uint32_t dockspaceId);
        void PreservePanelSizesOnResize(float workspaceWidth, float workspaceHeight);
        void DrawViewportPanel();
        void DrawSceneHierarchyPanel();
        void DrawObjectInspectorPanel();
        void DrawResourceBrowserPanel();
        void DrawTerminalPanel();

    private:
        static constexpr uint32_t SrvDescriptorCapacity = 64;
        static constexpr uint32_t DefaultEditorWidth    = 1920;
        static constexpr uint32_t DefaultEditorHeight   = 1080;

        std::unique_ptr<Core::Window> m_window;
        Renderer::SwapChain           m_swapChain;
        Renderer::CommandContext      m_commandContext;
        ComPtr<ID3D12DescriptorHeap>  m_srvHeap;
        UINT                          m_srvDescriptorSize{ 0 };
        std::vector<uint32_t>         m_freeSrvIndices;

        ImGuiContext*                 m_imguiContext{ nullptr };
        ImFont*                       m_uiFont{ nullptr };
        ImFont*                       m_monoFont{ nullptr };
        bool                          m_isInitialised{ false };
        bool                          m_resetDockLayout{ true };
        bool                          m_showSceneHierarchy{ true };
        bool                          m_showObjectInspector{ true };
        bool                          m_showResourceBrowser{ true };
        bool                          m_showTerminal{ true };

        uint32_t                      m_rightColumnNodeId{ 0 };
        uint32_t                      m_terminalNodeId{ 0 };
        uint32_t                      m_resourceBrowserNodeId{ 0 };
        float                         m_lastWorkspaceWidth{ 0.0f };
        float                         m_lastWorkspaceHeight{ 0.0f };

        std::vector<std::string>      m_terminalLines;
        std::string                   m_terminalInputBuffer;
    };
}
