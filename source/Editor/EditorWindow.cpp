// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "EditorWindow.h"
#include "EditorContext.h"
#include "EditorWidgets.h"
#include "Core/Logger.h"
#include "Engine/Scene.h"
#include "Maths/Quat.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <dwmapi.h>
#include <filesystem>
#include <knownfolders.h>
#include <shlobj.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Sandbox3D::Editor
{
    namespace
    {
        constexpr const char* DockspaceHostName      = "Sandbox3D_EditorDockspaceHost";
        constexpr const char* DockspaceName          = "Sandbox3D_EditorDockspace";
        constexpr const char* ViewportTitle          = "Viewport";
        constexpr const char* SceneHierarchyTitle    = "Scene Hierarchy";
        constexpr const char* ObjectInspectorTitle   = "Object Inspector";
        constexpr const char* ResourceBrowserTitle   = "Resource Browser";
        constexpr const char* TerminalTitle          = "Terminal";

        // Proportions matching the editorial viewport, hierarchy, inspector, resource, and terminal layout
        constexpr float LeftColumnFraction           = 0.60f;
        constexpr float TerminalHeightFraction       = 0.25f;
        constexpr float ResourceBrowserHeightFraction = 0.55f;
        constexpr float InspectorWidthFraction       = 0.50f;

        constexpr float MenuBarPaddingX              = 12.0f;
        constexpr float MenuBarPaddingY              = 6.0f;
        constexpr float BaseUiFontSize               = 16.0f;
        constexpr float BaseMonoFontSize             = 12.5f;
        constexpr float StandardDpi                  = 96.0f;
        constexpr COLORREF DarkTitleBarCaptionColour = RGB(17, 17, 20);
        constexpr float ClearColour[4]               = { 0.067f, 0.067f, 0.078f, 1.0f };

        void ImGuiSrvAlloc(
            ImGui_ImplDX12_InitInfo* info,
            D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle,
            D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle
        )
        {
            auto* editorWindow = static_cast<EditorWindow*>(info->UserData);
            editorWindow->AllocateSrvDescriptor(outCpuHandle, outGpuHandle);
        }

        void ImGuiSrvFree(
            ImGui_ImplDX12_InitInfo* info,
            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
            D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle
        )
        {
            auto* editorWindow = static_cast<EditorWindow*>(info->UserData);
            editorWindow->FreeSrvDescriptor(cpuHandle, gpuHandle);
        }
    }

    EditorWindow::~EditorWindow()
    {
        // Explicit shutdown should be invoked with the command queue prior to destruction
        if (m_isInitialised)
        {
            if (m_imguiContext)
            {
                ImGui::SetCurrentContext(m_imguiContext);
                ImGui_ImplDX12_Shutdown();
                ImGui_ImplWin32_Shutdown();
                ImGui::DestroyContext(m_imguiContext);
                m_imguiContext = nullptr;
            }
            m_srvHeap.Reset();
            m_swapChain.Shutdown();
            m_window.reset();
            m_isInitialised = false;
        }
    }

    void EditorWindow::Initialise(
        IDXGIFactory6* factory,
        ID3D12Device* device,
        ID3D12CommandQueue* commandQueue,
        const std::wstring& gpuDescription,
        const Renderer::Renderer* sourceRenderer
    )
    {
        if (m_isInitialised)
        {
            return;
        }

        Core::WindowOptions options{};
        options.resizable = true;
        options.maximised = true;
        options.className = L"Sandbox3D_EditorWindowClass";

        m_window = std::make_unique<Core::Window>(
            DefaultEditorWidth,
            DefaultEditorHeight,
            L"Sandbox3D - [Editor]",
            options
        );

        m_window->SetMessageInterceptor([this](HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& outResult) -> bool
        {
            if (m_imguiContext)
            {
                ImGui::SetCurrentContext(m_imguiContext);
                if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
                {
                    outResult = 1;
                    return true;
                }
            }
            return false;
        });

        m_window->SetResizeCallback([this, device, commandQueue](uint32_t newWidth, uint32_t newHeight)
        {
            OnResize(device, commandQueue, newWidth, newHeight);
        });

        m_swapChain.Initialise(
            factory,
            device,
            commandQueue,
            m_window->GetHwnd(),
            m_window->GetWidth(),
            m_window->GetHeight(),
            /* allowTearing = */ false
        );

        EnableDarkTitleBar();
        m_commandContext.Initialise(device);
        CreateSrvDescriptorHeap(device);
        InitialiseImGui(device, commandQueue);

        m_viewportPanel.Initialise(
            factory,
            device,
            commandQueue,
            gpuDescription,
            sourceRenderer,
            [this](D3D12_CPU_DESCRIPTOR_HANDLE* outCpu, D3D12_GPU_DESCRIPTOR_HANDLE* outGpu)
            {
                AllocateSrvDescriptor(outCpu, outGpu);
            }
        );

        m_isInitialised = true;
    }

    void EditorWindow::Shutdown(ID3D12CommandQueue* commandQueue) noexcept
    {
        if (!m_isInitialised)
        {
            return;
        }

        m_commandContext.Flush(commandQueue);
        m_viewportPanel.Shutdown(
            commandQueue,
            [this](D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle)
            {
                FreeSrvDescriptor(cpuHandle, gpuHandle);
            }
        );

        if (m_imguiContext)
        {
            ImGui::SetCurrentContext(m_imguiContext);
            ImGui_ImplDX12_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext(m_imguiContext);
            m_imguiContext = nullptr;
        }

        m_srvHeap.Reset();
        m_freeSrvIndices.clear();
        m_commandContext.Shutdown(commandQueue);
        m_swapChain.Shutdown();
        m_window.reset();
        m_isInitialised = false;
    }

    void EditorWindow::RenderViewport(
        ID3D12Device* device,
        ID3D12CommandQueue* commandQueue,
        std::span<const Renderer::RenderItem> renderItems,
        std::span<const Renderer::GpuLight> lights,
        float deltaTime,
        float updatesPerSecond,
        float targetUps
    )
    {
        if (!m_isInitialised || !m_window || m_window->IsMinimized())
        {
            return;
        }

        if (m_viewportPanel.NeedsResize())
        {
            m_commandContext.Flush(commandQueue);
        }

        m_viewportPanel.Render(
            device,
            commandQueue,
            renderItems,
            lights,
            deltaTime,
            updatesPerSecond,
            targetUps,
            m_window->IsFocused(),
            m_imguiContext,
            [this](D3D12_CPU_DESCRIPTOR_HANDLE* outCpu, D3D12_GPU_DESCRIPTOR_HANDLE* outGpu)
            {
                AllocateSrvDescriptor(outCpu, outGpu);
            }
        );
    }

    void EditorWindow::Render(ID3D12CommandQueue* commandQueue, bool vSync)
    {
        if (!m_isInitialised || !m_window || m_window->IsMinimized())
        {
            return;
        }

        m_swapChain.WaitForFrameLatency();
        const UINT backBufferIndex = m_swapChain.GetCurrentBackBufferIndex();
        m_commandContext.BeginFrame(backBufferIndex);

        ImGui::SetCurrentContext(m_imguiContext);
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        BuildWorkspace();

        ImGui::Render();

        ID3D12GraphicsCommandList* commandList = m_commandContext.GetCommandList();
        ID3D12Resource* backBuffer = m_swapChain.GetCurrentRenderTarget();

        D3D12_RESOURCE_BARRIER toRenderTarget{};
        toRenderTarget.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toRenderTarget.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        toRenderTarget.Transition.pResource   = backBuffer;
        toRenderTarget.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        toRenderTarget.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        toRenderTarget.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
        commandList->ResourceBarrier(1, &toRenderTarget);

        const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_swapChain.GetCurrentRtvHandle();
        commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
        commandList->ClearRenderTargetView(rtvHandle, ClearColour, 0, nullptr);

        ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
        commandList->SetDescriptorHeaps(1, heaps);

        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

        D3D12_RESOURCE_BARRIER toPresent{};
        toPresent.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toPresent.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        toPresent.Transition.pResource   = backBuffer;
        toPresent.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        toPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        toPresent.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
        commandList->ResourceBarrier(1, &toPresent);

        m_commandContext.Execute(commandQueue, backBufferIndex);
        m_swapChain.Present(vSync);
    }

    void EditorWindow::AppendTerminalLog(std::string entry)
    {
        m_terminalPanel.AppendLog(std::move(entry));
    }

    bool EditorWindow::IsRunning() const noexcept
    {
        return m_window && m_window->IsRunning();
    }

    bool EditorWindow::IsMinimized() const noexcept
    {
        return !m_window || m_window->IsMinimized();
    }

    bool EditorWindow::IsFocused() const noexcept
    {
        return m_window && m_window->IsFocused();
    }

    void EditorWindow::AllocateSrvDescriptor(
        D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle
    )
    {
        if (m_freeSrvIndices.empty())
        {
            HR_CHECK(E_OUTOFMEMORY);
        }

        const uint32_t index = m_freeSrvIndices.back();
        m_freeSrvIndices.pop_back();

        const D3D12_CPU_DESCRIPTOR_HANDLE cpuStart = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
        const D3D12_GPU_DESCRIPTOR_HANDLE gpuStart = m_srvHeap->GetGPUDescriptorHandleForHeapStart();

        outCpuHandle->ptr = cpuStart.ptr + static_cast<SIZE_T>(index) * m_srvDescriptorSize;
        outGpuHandle->ptr = gpuStart.ptr + static_cast<UINT64>(index) * m_srvDescriptorSize;
    }

    void EditorWindow::FreeSrvDescriptor(
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle
    ) noexcept
    {
        (void)gpuHandle;
        if (!m_srvHeap || m_srvDescriptorSize == 0)
        {
            return;
        }

        const SIZE_T heapStart = m_srvHeap->GetCPUDescriptorHandleForHeapStart().ptr;
        if (cpuHandle.ptr < heapStart)
        {
            return;
        }

        const uint32_t index = static_cast<uint32_t>((cpuHandle.ptr - heapStart) / m_srvDescriptorSize);
        if (index < SrvDescriptorCapacity)
        {
            m_freeSrvIndices.push_back(index);
        }
    }

    void EditorWindow::CreateSrvDescriptorHeap(ID3D12Device* device)
    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = SrvDescriptorCapacity;
        heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        heapDesc.NodeMask       = 0;

        HR_CHECK(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_srvHeap)));
        m_srvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        m_freeSrvIndices.clear();
        m_freeSrvIndices.reserve(SrvDescriptorCapacity);
        for (uint32_t i = SrvDescriptorCapacity; i > 0; --i)
        {
            m_freeSrvIndices.push_back(i - 1);
        }
    }

    void EditorWindow::SetCameraPose(const Maths::Vec3D& position, double yaw, double pitch) noexcept
    {
        m_viewportPanel.SetCameraPose(position, yaw, pitch);
    }

    void EditorWindow::SetScene(Engine::Scene* scene) noexcept
    {
        m_context.scene            = scene;
        m_context.selectedObjectId = 0;
        m_sceneHierarchyPanel.ResetTransientState();
    }

    std::shared_ptr<Engine::Base> EditorWindow::GetSelectedObject() const noexcept
    {
        if (!m_context.scene || m_context.selectedObjectId == 0)
        {
            return nullptr;
        }
        return m_context.scene->FindObject(m_context.selectedObjectId);
    }

    void EditorWindow::EnableDarkTitleBar() noexcept
    {
        if (!m_window || !m_window->GetHwnd())
        {
            return;
        }

        const HWND hwnd = m_window->GetHwnd();
        const BOOL useDarkMode = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));

        // Match the Windows 11 caption bar colour to the dark Editor menu bar
        constexpr DWORD DwmCaptionColorAttribute = 35;
        DwmSetWindowAttribute(hwnd, DwmCaptionColorAttribute, &DarkTitleBarCaptionColour, sizeof(DarkTitleBarCaptionColour));
    }

    void EditorWindow::LoadFonts(float dpiScale)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->Clear();

        std::filesystem::path fontsDirectory;
        PWSTR knownFontsPath = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Fonts, 0, nullptr, &knownFontsPath)) && knownFontsPath)
        {
            fontsDirectory = knownFontsPath;
        }
        if (knownFontsPath)
        {
            CoTaskMemFree(knownFontsPath);
        }

        constexpr std::array<const wchar_t*, 2> uiFontCandidates =
        {
            L"SegUIVar.ttf",
            L"segoeui.ttf"
        };

        const float scaledUiSize = std::round(BaseUiFontSize * dpiScale);
        if (!fontsDirectory.empty())
        {
            for (const wchar_t* filename : uiFontCandidates)
            {
                const std::filesystem::path candidate = fontsDirectory / filename;
                if (std::filesystem::exists(candidate))
                {
                    const std::string utf8Path = candidate.string();
                    m_uiFont = io.Fonts->AddFontFromFileTTF(utf8Path.c_str(), scaledUiSize);
                    if (m_uiFont)
                    {
                        break;
                    }
                }
            }

            const std::filesystem::path italicCandidate = fontsDirectory / L"segoeuii.ttf";
            if (std::filesystem::exists(italicCandidate))
            {
                const std::string utf8Path = italicCandidate.string();
                m_italicFont = io.Fonts->AddFontFromFileTTF(utf8Path.c_str(), scaledUiSize);
            }
        }

        if (!m_uiFont)
        {
            ImFontConfig defaultConfig{};
            defaultConfig.SizePixels = scaledUiSize;
            m_uiFont = io.Fonts->AddFontDefault(&defaultConfig);
        }

        const float scaledMonoSize = std::round(BaseMonoFontSize * dpiScale);
        if (!fontsDirectory.empty())
        {
            const std::filesystem::path monoPath = fontsDirectory / L"consola.ttf";
            if (std::filesystem::exists(monoPath))
            {
                const std::string utf8Path = monoPath.string();
                m_monoFont = io.Fonts->AddFontFromFileTTF(utf8Path.c_str(), scaledMonoSize);
            }
        }

        if (!m_monoFont)
        {
            m_monoFont = m_uiFont;
        }
    }

    void EditorWindow::InitialiseImGui(ID3D12Device* device, ID3D12CommandQueue* commandQueue)
    {
        IMGUI_CHECKVERSION();
        m_imguiContext = ImGui::CreateContext();
        ImGui::SetCurrentContext(m_imguiContext);

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.IniFilename = nullptr;

        const UINT windowDpi = GetDpiForWindow(m_window->GetHwnd());
        const float dpiScale = (windowDpi > 0) ? (static_cast<float>(windowDpi) / StandardDpi) : 1.0f;

        LoadFonts(dpiScale);
        ConfigureStyle(dpiScale);

        if (!ImGui_ImplWin32_Init(m_window->GetHwnd()))
        {
            HR_CHECK(E_FAIL);
        }

        ImGui_ImplDX12_InitInfo initInfo{};
        initInfo.Device             = device;
        initInfo.CommandQueue       = commandQueue;
        initInfo.NumFramesInFlight  = static_cast<int>(Renderer::SwapChain::BufferCount);
        initInfo.RTVFormat          = Renderer::SwapChain::BackBufferFormat;
        initInfo.DSVFormat          = DXGI_FORMAT_UNKNOWN;
        initInfo.UserData           = this;
        initInfo.SrvDescriptorHeap  = m_srvHeap.Get();
        initInfo.SrvDescriptorAllocFn = &ImGuiSrvAlloc;
        initInfo.SrvDescriptorFreeFn  = &ImGuiSrvFree;

        if (!ImGui_ImplDX12_Init(&initInfo))
        {
            HR_CHECK(E_FAIL);
        }
    }

    void EditorWindow::ConfigureStyle(float dpiScale)
    {
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();

        style.WindowRounding    = 6.0f;
        style.ChildRounding     = 6.0f;
        style.PopupRounding     = 6.0f;
        style.TabRounding       = 5.0f;
        style.FrameRounding     = 6.0f;
        style.ScrollbarRounding = 6.0f;
        style.WindowBorderSize  = 1.0f;
        style.ChildBorderSize   = 1.0f;
        style.FrameBorderSize   = 1.0f;
        style.WindowPadding     = ImVec2(10.0f, 10.0f);
        style.FramePadding      = ImVec2(8.0f, 5.0f);
        style.ItemSpacing       = ImVec2(8.0f, 6.0f);
        style.ScaleAllSizes(dpiScale);
        style.TabBarOverlineSize = 0.0f;

        ImVec4* colours = style.Colors;
        colours[ImGuiCol_Text]                  = ImVec4(0.925f, 0.933f, 0.949f, 1.00f);
        colours[ImGuiCol_TextDisabled]          = ImVec4(0.557f, 0.573f, 0.608f, 1.00f);
        colours[ImGuiCol_WindowBg]              = ImVec4(0.067f, 0.067f, 0.078f, 1.00f);
        colours[ImGuiCol_ChildBg]               = ImVec4(0.082f, 0.082f, 0.102f, 1.00f);
        colours[ImGuiCol_PopupBg]               = ImVec4(0.082f, 0.082f, 0.102f, 0.98f);
        colours[ImGuiCol_Border]                = ImVec4(0.149f, 0.149f, 0.180f, 1.00f);
        colours[ImGuiCol_BorderShadow]          = ImVec4(0.000f, 0.000f, 0.000f, 0.00f);
        colours[ImGuiCol_FrameBg]               = ImVec4(0.082f, 0.082f, 0.102f, 1.00f);
        colours[ImGuiCol_FrameBgHovered]        = ImVec4(0.114f, 0.114f, 0.141f, 1.00f);
        colours[ImGuiCol_FrameBgActive]         = ImVec4(0.145f, 0.145f, 0.180f, 1.00f);
        colours[ImGuiCol_TitleBg]               = ImVec4(0.082f, 0.082f, 0.102f, 1.00f);
        colours[ImGuiCol_TitleBgActive]         = ImVec4(0.082f, 0.082f, 0.102f, 1.00f);
        colours[ImGuiCol_MenuBarBg]             = ImVec4(0.000f, 0.000f, 0.000f, 1.00f);
        colours[ImGuiCol_ScrollbarBg]           = ImVec4(0.067f, 0.067f, 0.078f, 1.00f);
        colours[ImGuiCol_ScrollbarGrab]         = ImVec4(0.169f, 0.169f, 0.208f, 1.00f);
        colours[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.231f, 0.231f, 0.286f, 1.00f);
        colours[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.294f, 0.294f, 0.361f, 1.00f);
        colours[ImGuiCol_CheckMark]             = ImVec4(0.169f, 0.498f, 1.000f, 1.00f);
        colours[ImGuiCol_SliderGrab]            = ImVec4(0.169f, 0.498f, 1.000f, 1.00f);
        colours[ImGuiCol_SliderGrabActive]      = ImVec4(0.282f, 0.569f, 1.000f, 1.00f);
        colours[ImGuiCol_Tab]                   = ImVec4(0.114f, 0.114f, 0.141f, 1.00f);
        colours[ImGuiCol_TabHovered]            = ImVec4(0.114f, 0.114f, 0.141f, 1.00f);
        colours[ImGuiCol_TabSelected]           = ImVec4(0.114f, 0.114f, 0.141f, 1.00f);
        colours[ImGuiCol_TabSelectedOverline]   = ImVec4(0.000f, 0.000f, 0.000f, 0.00f);
        colours[ImGuiCol_TabDimmed]             = ImVec4(0.114f, 0.114f, 0.141f, 1.00f);
        colours[ImGuiCol_TabDimmedSelected]     = ImVec4(0.114f, 0.114f, 0.141f, 1.00f);
        colours[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.000f, 0.000f, 0.000f, 0.00f);
        colours[ImGuiCol_Header]                = ImVec4(0.114f, 0.114f, 0.141f, 1.00f);
        colours[ImGuiCol_HeaderHovered]         = ImVec4(0.149f, 0.149f, 0.184f, 1.00f);
        colours[ImGuiCol_HeaderActive]          = ImVec4(0.180f, 0.180f, 0.227f, 1.00f);
        colours[ImGuiCol_Button]                = ImVec4(0.114f, 0.114f, 0.141f, 1.00f);
        colours[ImGuiCol_ButtonHovered]         = ImVec4(0.165f, 0.165f, 0.204f, 1.00f);
        colours[ImGuiCol_ButtonActive]          = ImVec4(0.169f, 0.498f, 1.000f, 1.00f);
        colours[ImGuiCol_Separator]             = ImVec4(0.149f, 0.149f, 0.180f, 1.00f);
        colours[ImGuiCol_DockingPreview]        = ImVec4(0.169f, 0.498f, 1.000f, 0.55f);
    }

    void EditorWindow::OnResize(
        ID3D12Device* device,
        ID3D12CommandQueue* commandQueue,
        uint32_t width,
        uint32_t height
    )
    {
        if (!m_isInitialised || width == 0 || height == 0)
        {
            return;
        }

        m_commandContext.Flush(commandQueue);
        m_swapChain.Resize(device, width, height);
    }

    void EditorWindow::BuildWorkspace()
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        constexpr ImGuiWindowFlags hostFlags =
            ImGuiWindowFlags_MenuBar |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin(DockspaceHostName, nullptr, hostFlags);
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor();

        DrawMainMenuBar();

        const ImGuiID dockspaceId = ImGui::GetID(DockspaceName);

        if (!m_resetDockLayout)
        {
            PreservePanelSizesOnResize(viewport->WorkSize.x, viewport->WorkSize.y);
        }

        ImGui::DockSpace(
            dockspaceId,
            ImVec2(0.0f, 0.0f),
            ImGuiDockNodeFlags_PassthruCentralNode |
                ImGuiDockNodeFlags_NoWindowMenuButton |
                ImGuiDockNodeFlags_NoCloseButton |
                ImGuiDockNodeFlags_NoUndocking
        );

        const bool justResetLayout = m_resetDockLayout;
        if (m_resetDockLayout)
        {
            BuildDefaultDockLayout(dockspaceId);
            m_lastWorkspaceWidth  = viewport->WorkSize.x;
            m_lastWorkspaceHeight = viewport->WorkSize.y;
            m_resetDockLayout = false;
        }

        ImGui::End();

        m_viewportPanel.Draw();

        if (m_showSceneHierarchy)
        {
            m_sceneHierarchyPanel.Draw(m_context, m_showSceneHierarchy, m_italicFont);
        }
        if (m_showObjectInspector)
        {
            m_objectInspectorPanel.Draw(m_context, m_showObjectInspector, m_italicFont);
        }
        if (m_showResourceBrowser)
        {
            m_resourceBrowserPanel.Draw(m_showResourceBrowser);
        }
        if (m_showTerminal)
        {
            m_terminalPanel.Draw(m_showTerminal, m_monoFont);
        }

        if (justResetLayout)
        {
            ImGui::SetWindowFocus(nullptr);
            m_viewportPanel.ClearFocus();
        }
    }

    void EditorWindow::DrawMainMenuBar()
    {
        ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(MenuBarPaddingX, MenuBarPaddingY));

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Sandbox3D"))
            {
                CompactMenuItem("About Sandbox3D", nullptr, false);
                ImGui::Separator();
                if (CompactMenuItem("Exit"))
                {
                    if (m_window && m_window->GetHwnd())
                    {
                        PostMessageW(m_window->GetHwnd(), WM_CLOSE, 0, 0);
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("File"))
            {
                CompactMenuItem("New Scene", "Ctrl+N", false);
                CompactMenuItem("Open Scene...", "Ctrl+O", false);
                CompactMenuItem("Save Scene", "Ctrl+S", false);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                const auto drawViewMenuItem = [](PanelIcon icon, const char* title, bool* pSelected)
                {
                    const std::string paddedLabel = std::string("       ") + title;
                    ImGui::MenuItem(paddedLabel.c_str(), nullptr, pSelected);

                    const ImVec2 itemMin  = ImGui::GetItemRectMin();
                    const ImVec2 itemMax  = ImGui::GetItemRectMax();
                    const float iconSize  = ImGui::GetFontSize() * 0.80f;
                    const float leftPad   = ImGui::GetStyle().FramePadding.x;
                    const ImVec2 center(itemMin.x + leftPad + iconSize * 0.5f, (itemMin.y + itemMax.y) * 0.5f);
                    DrawPanelIcon(ImGui::GetWindowDrawList(), icon, center, iconSize, PanelIconColour);
                };

                drawViewMenuItem(PanelIcon::SceneHierarchy, SceneHierarchyTitle, &m_showSceneHierarchy);
                drawViewMenuItem(PanelIcon::ObjectInspector, ObjectInspectorTitle, &m_showObjectInspector);
                drawViewMenuItem(PanelIcon::ResourceBrowser, ResourceBrowserTitle, &m_showResourceBrowser);
                drawViewMenuItem(PanelIcon::Terminal, TerminalTitle, &m_showTerminal);
                ImGui::Separator();
                if (ImGui::MenuItem("Reset Layout"))
                {
                    m_showSceneHierarchy  = true;
                    m_showObjectInspector = true;
                    m_showResourceBrowser = true;
                    m_showTerminal        = true;
                    m_resetDockLayout     = true;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Objects"))
            {
                const bool hasScene = (m_context.scene != nullptr);
                if (CompactMenuItem("Create Empty Object", "Ctrl+Shift+N", hasScene))
                {
                    m_sceneHierarchyPanel.CreateObject(m_context, CreateObjectKind::EmptyObject);
                }
                if (CompactMenuItem("Create Light", "Ctrl+Shift+L", hasScene))
                {
                    m_sceneHierarchyPanel.CreateObject(m_context, CreateObjectKind::Light);
                }
                if (CompactMenuItem("Create Camera", "Ctrl+Shift+C", hasScene))
                {
                    m_sceneHierarchyPanel.CreateObject(m_context, CreateObjectKind::Camera);
                }
                ImGui::Separator();
                if (ImGui::BeginMenu("Create Object", hasScene))
                {
                    constexpr std::array<SearchableOption, 5> createOptions =
                    {{
                        { "Body",          false },
                        { "Camera",        false },
                        { "Character",     false },
                        { "Light",         false },
                        { "TerrainObject", false }
                    }};
                    constexpr std::array<CreateObjectKind, 5> createKinds =
                    {
                        CreateObjectKind::Body,
                        CreateObjectKind::Camera,
                        CreateObjectKind::Character,
                        CreateObjectKind::Light,
                        CreateObjectKind::TerrainObject
                    };

                    const int picked = DrawSearchablePickerList("##CreateObjectPicker", createOptions, 4, m_italicFont);
                    if (picked >= 0 && picked < static_cast<int>(createKinds.size()))
                    {
                        m_sceneHierarchyPanel.CreateObject(m_context, createKinds[picked]);
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        const ImGuiIO& io = ImGui::GetIO();
        if (m_context.scene && !io.WantTextInput && io.KeyCtrl && io.KeyShift && !io.KeyAlt)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_N, false))
            {
                m_sceneHierarchyPanel.CreateObject(m_context, CreateObjectKind::EmptyObject);
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_L, false))
            {
                m_sceneHierarchyPanel.CreateObject(m_context, CreateObjectKind::Light);
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_C, false))
            {
                m_sceneHierarchyPanel.CreateObject(m_context, CreateObjectKind::Camera);
            }
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
    }

    void EditorWindow::BuildDefaultDockLayout(uint32_t dockspaceId)
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

        ImGuiID leftColumnId  = 0;
        ImGuiID rightColumnId = 0;
        ImGui::DockBuilderSplitNode(
            dockspaceId,
            ImGuiDir_Left,
            LeftColumnFraction,
            &leftColumnId,
            &rightColumnId
        );

        ImGuiID terminalId    = 0;
        ImGuiID viewportId    = 0;
        ImGui::DockBuilderSplitNode(
            leftColumnId,
            ImGuiDir_Down,
            TerminalHeightFraction,
            &terminalId,
            &viewportId
        );

        ImGuiID resourceBrowserId = 0;
        ImGuiID rightTopId        = 0;
        ImGui::DockBuilderSplitNode(
            rightColumnId,
            ImGuiDir_Down,
            ResourceBrowserHeightFraction,
            &resourceBrowserId,
            &rightTopId
        );

        ImGuiID objectInspectorId = 0;
        ImGuiID sceneHierarchyId  = 0;
        ImGui::DockBuilderSplitNode(
            rightTopId,
            ImGuiDir_Right,
            InspectorWidthFraction,
            &objectInspectorId,
            &sceneHierarchyId
        );

        ImGui::DockBuilderDockWindow(ViewportTitle, viewportId);
        ImGui::DockBuilderDockWindow(TerminalTitle, terminalId);
        ImGui::DockBuilderDockWindow(SceneHierarchyTitle, sceneHierarchyId);
        ImGui::DockBuilderDockWindow(ObjectInspectorTitle, objectInspectorId);
        ImGui::DockBuilderDockWindow(ResourceBrowserTitle, resourceBrowserId);

        if (ImGuiDockNode* sceneNode = ImGui::DockBuilderGetNode(sceneHierarchyId))
        {
            sceneNode->SetLocalFlags(sceneNode->LocalFlags & ~ImGuiDockNodeFlags_CentralNode);
        }

        if (ImGuiDockNode* viewportNode = ImGui::DockBuilderGetNode(viewportId))
        {
            viewportNode->SetLocalFlags(viewportNode->LocalFlags | ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_CentralNode);
            if (ImGuiDockNode* rootNode = ImGui::DockBuilderGetNode(dockspaceId))
            {
                rootNode->CentralNode = viewportNode;
            }
        }

        m_rightColumnNodeId     = rightColumnId;
        m_terminalNodeId        = terminalId;
        m_resourceBrowserNodeId = resourceBrowserId;

        ImGui::DockBuilderFinish(dockspaceId);
    }

    void EditorWindow::PreservePanelSizesOnResize(float workspaceWidth, float workspaceHeight)
    {
        if (workspaceWidth <= 0.0f || workspaceHeight <= 0.0f)
        {
            return;
        }

        const bool widthChanged  = (workspaceWidth != m_lastWorkspaceWidth);
        const bool heightChanged = (workspaceHeight != m_lastWorkspaceHeight);

        if (widthChanged)
        {
            if (ImGuiDockNode* rightNode = ImGui::DockBuilderGetNode(m_rightColumnNodeId))
            {
                rightNode->WantLockSizeOnce = true;
            }
            m_lastWorkspaceWidth = workspaceWidth;
        }

        if (heightChanged)
        {
            if (ImGuiDockNode* terminalNode = ImGui::DockBuilderGetNode(m_terminalNodeId))
            {
                terminalNode->WantLockSizeOnce = true;
            }
            if (ImGuiDockNode* resourceNode = ImGui::DockBuilderGetNode(m_resourceBrowserNodeId))
            {
                resourceNode->WantLockSizeOnce = true;
            }
            m_lastWorkspaceHeight = workspaceHeight;
        }
    }
}
