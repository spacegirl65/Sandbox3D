// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "EditorWindow.h"
#include "Maths/Quat.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <dwmapi.h>
#include <filesystem>

#pragma comment(lib, "dwmapi.lib")

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
        constexpr float BaseMonoFontSize             = 14.5f;
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

        m_viewportRenderer.InitialiseOffscreen(
            factory,
            device,
            commandQueue,
            m_viewportWidth,
            m_viewportHeight,
            gpuDescription
        );
        if (sourceRenderer)
        {
            m_viewportRenderer.CopyEnvironmentSettingsFrom(*sourceRenderer);
        }

        const float initialAspect = static_cast<float>(m_viewportWidth) / static_cast<float>(m_viewportHeight);
        m_editorCamera.SetPerspective(Maths::ToRadians(60.0f), initialAspect, 0.1f, 30000.0f);
        UpdateViewportCamera(0.0f);
        m_viewportRenderer.SetCamera(&m_editorCamera);
        UpdateViewportSrv(device);

        AppendTerminalLog("[Editor] Sandbox3D Editor initialised.");
        m_isInitialised = true;
    }

    void EditorWindow::Shutdown(ID3D12CommandQueue* commandQueue) noexcept
    {
        if (!m_isInitialised)
        {
            return;
        }

        m_commandContext.Flush(commandQueue);
        m_viewportRenderer.Shutdown(commandQueue);

        if (m_hasViewportSrv)
        {
            for (UINT i = 0; i < Renderer::SwapChain::BufferCount; ++i)
            {
                FreeSrvDescriptor(m_viewportSrvCpuHandles[i], m_viewportSrvGpuHandles[i]);
                m_viewportSrvCpuHandles[i] = {};
                m_viewportSrvGpuHandles[i] = {};
            }
            m_hasViewportSrv = false;
        }

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

        if (m_desiredViewportWidth > 0 && m_desiredViewportHeight > 0 &&
            (m_desiredViewportWidth != m_viewportWidth || m_desiredViewportHeight != m_viewportHeight))
        {
            m_commandContext.Flush(commandQueue);
            m_viewportWidth  = m_desiredViewportWidth;
            m_viewportHeight = m_desiredViewportHeight;
            m_viewportRenderer.OnResize(device, commandQueue, m_viewportWidth, m_viewportHeight);
            UpdateViewportSrv(device);
        }

        UpdateViewportCamera(deltaTime);

        m_viewportRenderer.Render(
            commandQueue,
            renderItems,
            lights,
            /* vSync = */ false,
            renderItems.size(),
            updatesPerSecond,
            targetUps
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
        m_terminalLines.push_back(std::move(entry));
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

    void EditorWindow::UpdateViewportSrv(ID3D12Device* device)
    {
        if (!device)
        {
            return;
        }

        if (!m_hasViewportSrv)
        {
            for (UINT i = 0; i < Renderer::SwapChain::BufferCount; ++i)
            {
                AllocateSrvDescriptor(&m_viewportSrvCpuHandles[i], &m_viewportSrvGpuHandles[i]);
            }
            m_hasViewportSrv = true;
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
            0, 1, 2, D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1
        );
        srvDesc.Format                        = Renderer::SwapChain::BackBufferFormat;
        srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip     = 0;
        srvDesc.Texture2D.MipLevels           = 1;
        srvDesc.Texture2D.PlaneSlice          = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

        for (UINT i = 0; i < Renderer::SwapChain::BufferCount; ++i)
        {
            ID3D12Resource* const offscreenResource = m_viewportRenderer.GetOffscreenResource(i);
            if (offscreenResource)
            {
                device->CreateShaderResourceView(offscreenResource, &srvDesc, m_viewportSrvCpuHandles[i]);
            }
        }
    }

    void EditorWindow::UpdateViewportCamera(float deltaTime)
    {
        constexpr double turnSpeed        = 1.0;
        constexpr double baseMoveSpeed    = 120.0;
        constexpr double zoomSpeed        = 120.0;
        constexpr double mouseSensitivity = 0.003;
        constexpr double pitchLimit       = 1.50;

        const double dt = std::clamp(static_cast<double>(deltaTime), 0.0, 0.1);

        Maths::QuatD cameraQuat = Maths::QuatD::FromEulerAngles(m_cameraPitch, m_cameraYaw, 0.0);
        Maths::Vec3D forward    = cameraQuat.Rotate(Maths::Vec3D::Forward());
        Maths::Vec3D right      = cameraQuat.Rotate(Maths::Vec3D::Right());

        if (m_imguiContext && m_window && m_window->IsFocused())
        {
            ImGui::SetCurrentContext(m_imguiContext);
            const ImGuiIO& io = ImGui::GetIO();

            if (!io.WantTextInput && (m_isViewportHovered || ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)))
            {
                if (m_isViewportHovered && ImGui::IsMouseDown(ImGuiMouseButton_Right))
                {
                    m_cameraYaw   += static_cast<double>(io.MouseDelta.x) * mouseSensitivity;
                    m_cameraPitch -= static_cast<double>(io.MouseDelta.y) * mouseSensitivity;
                    m_cameraPitch  = std::clamp(m_cameraPitch, -pitchLimit, pitchLimit);
                }

                // Arrow keys adjust yaw and pitch
                if (ImGui::IsKeyDown(ImGuiKey_LeftArrow))
                {
                    m_cameraYaw -= turnSpeed * dt;
                }
                if (ImGui::IsKeyDown(ImGuiKey_RightArrow))
                {
                    m_cameraYaw += turnSpeed * dt;
                }
                if (ImGui::IsKeyDown(ImGuiKey_UpArrow))
                {
                    m_cameraPitch = std::clamp(m_cameraPitch + turnSpeed * dt, -pitchLimit, pitchLimit);
                }
                if (ImGui::IsKeyDown(ImGuiKey_DownArrow))
                {
                    m_cameraPitch = std::clamp(m_cameraPitch - turnSpeed * dt, -pitchLimit, pitchLimit);
                }

                cameraQuat = Maths::QuatD::FromEulerAngles(m_cameraPitch, m_cameraYaw, 0.0);
                forward    = cameraQuat.Rotate(Maths::Vec3D::Forward());
                right      = cameraQuat.Rotate(Maths::Vec3D::Right());

                // WASD, Space/LeftShift free translation
                Maths::Vec3D moveDelta(0.0, 0.0, 0.0);
                if (ImGui::IsKeyDown(ImGuiKey_W))
                {
                    moveDelta += forward;
                }
                if (ImGui::IsKeyDown(ImGuiKey_S))
                {
                    moveDelta -= forward;
                }
                if (ImGui::IsKeyDown(ImGuiKey_D))
                {
                    moveDelta += right;
                }
                if (ImGui::IsKeyDown(ImGuiKey_A))
                {
                    moveDelta -= right;
                }
                if (ImGui::IsKeyDown(ImGuiKey_Space))
                {
                    moveDelta += Maths::Vec3D::Up();
                }
                if (ImGui::IsKeyDown(ImGuiKey_LeftShift))
                {
                    moveDelta -= Maths::Vec3D::Up();
                }

                if (moveDelta.LengthSquared() > 0.0)
                {
                    m_cameraPosition += moveDelta.Normalised() * (baseMoveSpeed * dt);
                }

                // Bracket keys dolly forward and backward
                if (ImGui::IsKeyDown(ImGuiKey_LeftBracket))
                {
                    m_cameraPosition += forward * (zoomSpeed * dt);
                }
                if (ImGui::IsKeyDown(ImGuiKey_RightBracket))
                {
                    m_cameraPosition -= forward * (zoomSpeed * dt);
                }
            }
        }

        const Maths::Vec3D up = cameraQuat.Rotate(Maths::Vec3D::Up());
        m_editorCamera.SetLookAt(m_cameraPosition, m_cameraPosition + forward, up);
    }

    void EditorWindow::SetCameraPose(const Maths::Vec3D& position, double yaw, double pitch) noexcept
    {
        constexpr double pitchLimit = 1.50;
        m_cameraPosition = position;
        m_cameraYaw      = yaw;
        m_cameraPitch    = std::clamp(pitch, -pitchLimit, pitchLimit);

        const Maths::QuatD cameraQuat = Maths::QuatD::FromEulerAngles(m_cameraPitch, m_cameraYaw, 0.0);
        const Maths::Vec3D forward    = cameraQuat.Rotate(Maths::Vec3D::Forward());
        const Maths::Vec3D up         = cameraQuat.Rotate(Maths::Vec3D::Up());
        m_editorCamera.SetLookAt(m_cameraPosition, m_cameraPosition + forward, up);
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

        constexpr std::array<const char*, 2> uiFontCandidates =
        {
            "C:\\Windows\\Fonts\\SegUIVar.ttf",
            "C:\\Windows\\Fonts\\segoeui.ttf"
        };

        const float scaledUiSize = std::round(BaseUiFontSize * dpiScale);
        for (const char* path : uiFontCandidates)
        {
            if (std::filesystem::exists(path))
            {
                m_uiFont = io.Fonts->AddFontFromFileTTF(path, scaledUiSize);
                if (m_uiFont)
                {
                    break;
                }
            }
        }

        if (!m_uiFont)
        {
            ImFontConfig defaultConfig{};
            defaultConfig.SizePixels = scaledUiSize;
            m_uiFont = io.Fonts->AddFontDefault(&defaultConfig);
        }

        constexpr const char* monoFontPath = "C:\\Windows\\Fonts\\consola.ttf";
        const float scaledMonoSize = std::round(BaseMonoFontSize * dpiScale);
        if (std::filesystem::exists(monoFontPath))
        {
            m_monoFont = io.Fonts->AddFontFromFileTTF(monoFontPath, scaledMonoSize);
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
        colours[ImGuiCol_Tab]                   = ImVec4(0.082f, 0.082f, 0.102f, 1.00f);
        colours[ImGuiCol_TabHovered]            = ImVec4(0.145f, 0.145f, 0.180f, 1.00f);
        colours[ImGuiCol_TabSelected]           = ImVec4(0.114f, 0.114f, 0.141f, 1.00f);
        colours[ImGuiCol_TabDimmed]             = ImVec4(0.067f, 0.067f, 0.078f, 1.00f);
        colours[ImGuiCol_TabDimmedSelected]     = ImVec4(0.082f, 0.082f, 0.102f, 1.00f);
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

        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

        if (m_resetDockLayout)
        {
            BuildDefaultDockLayout(dockspaceId);
            m_lastWorkspaceWidth  = viewport->WorkSize.x;
            m_lastWorkspaceHeight = viewport->WorkSize.y;
            m_resetDockLayout = false;
        }

        ImGui::End();

        DrawViewportPanel();

        if (m_showSceneHierarchy)
        {
            DrawSceneHierarchyPanel();
        }
        if (m_showObjectInspector)
        {
            DrawObjectInspectorPanel();
        }
        if (m_showResourceBrowser)
        {
            DrawResourceBrowserPanel();
        }
        if (m_showTerminal)
        {
            DrawTerminalPanel();
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
                ImGui::MenuItem("About Sandbox3D", nullptr, false, false);
                ImGui::Separator();
                if (ImGui::MenuItem("Exit"))
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
                ImGui::MenuItem("New Scene", "Ctrl+N", false, false);
                ImGui::MenuItem("Open Scene...", "Ctrl+O", false, false);
                ImGui::MenuItem("Save Scene", "Ctrl+S", false, false);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem(SceneHierarchyTitle, nullptr, &m_showSceneHierarchy);
                ImGui::MenuItem(ObjectInspectorTitle, nullptr, &m_showObjectInspector);
                ImGui::MenuItem(ResourceBrowserTitle, nullptr, &m_showResourceBrowser);
                ImGui::MenuItem(TerminalTitle, nullptr, &m_showTerminal);
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
                ImGui::MenuItem("Create Empty Object", nullptr, false, false);
                ImGui::MenuItem("Create Light", nullptr, false, false);
                ImGui::MenuItem("Create Camera", nullptr, false, false);
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
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

        if (ImGuiDockNode* viewportNode = ImGui::DockBuilderGetNode(viewportId))
        {
            viewportNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
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

    void EditorWindow::DrawViewportToolbar()
    {
        const float frameHeight    = ImGui::GetFrameHeight();
        const float toolbarHeight  = frameHeight + 12.0f;
        const float buttonWidth    = frameHeight + 6.0f;
        const float buttonHeight   = frameHeight + 2.0f;
        const float containerRound = 6.0f;
        constexpr int buttonCount  = 3;
        const float groupWidth     = buttonWidth * buttonCount;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.047f, 0.047f, 0.000f, 1.00f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

        if (ImGui::BeginChild("ViewportToolbar", ImVec2(0.0f, toolbarHeight), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 barMin  = ImGui::GetWindowPos();
            const ImVec2 barMax  = ImVec2(barMin.x + ImGui::GetWindowWidth(), barMin.y + toolbarHeight);

            const float startX = std::max(8.0f, (ImGui::GetWindowWidth() - groupWidth) * 0.5f);
            const float startY = (toolbarHeight - buttonHeight) * 0.5f;

            // Encapsulate the three playback controls inside a unified segmented container
            const ImVec2 groupScreenMin(barMin.x + startX, barMin.y + startY);
            const ImVec2 groupScreenMax(groupScreenMin.x + groupWidth, groupScreenMin.y + buttonHeight);
            drawList->AddRectFilled(groupScreenMin, groupScreenMax, IM_COL32(21, 21, 26, 255), containerRound);

            const auto drawToolbarButton = [&](
                const char* id,
                const char* tooltip,
                bool enabled,
                bool active,
                ImDrawFlags cornerFlags,
                ImU32 symbolActiveCol,
                ImU32 symbolIdleCol,
                auto&& drawSymbolFn
            ) -> bool
            {
                const ImVec2 btnMin = ImGui::GetCursorScreenPos();
                const ImVec2 btnMax = ImVec2(btnMin.x + buttonWidth, btnMin.y + buttonHeight);

                bool clicked = false;
                if (enabled)
                {
                    clicked = ImGui::InvisibleButton(id, ImVec2(buttonWidth, buttonHeight));
                }
                else
                {
                    ImGui::Dummy(ImVec2(buttonWidth, buttonHeight));
                }

                const bool hovered = enabled && ImGui::IsItemHovered();
                const bool held    = enabled && ImGui::IsItemActive();

                if (active || held || hovered)
                {
                    ImU32 bgCol = IM_COL32(31, 32, 41, 255);
                    if (active)
                    {
                        bgCol = IM_COL32(38, 42, 54, 255);
                    }
                    else if (held)
                    {
                        bgCol = IM_COL32(42, 44, 56, 255);
                    }
                    drawList->AddRectFilled(btnMin, btnMax, bgCol, containerRound, cornerFlags);
                }

                const ImU32 disabledCol = IM_COL32(82, 85, 96, 255);
                const ImU32 symbolCol   = !enabled ? disabledCol : (active || hovered ? symbolActiveCol : symbolIdleCol);
                drawSymbolFn(drawList, btnMin, buttonWidth, buttonHeight, symbolCol);

                if (hovered)
                {
                    ImGui::SetTooltip("%s", tooltip);
                }

                return clicked;
            };

            const bool isStopped = (m_playState == PlayState::Stopped);
            const bool isPlaying = (m_playState == PlayState::Playing);
            const bool isPaused  = (m_playState == PlayState::Paused);

            ImGui::SetCursorPos(ImVec2(startX, startY));
            if (drawToolbarButton(
                "##ToolbarPlay",
                isPaused ? "Resume" : "Play",
                !isPlaying,
                isPlaying,
                ImDrawFlags_RoundCornersLeft,
                IM_COL32(64, 214, 108, 255),
                IM_COL32(52, 184, 92, 255),
                [](ImDrawList* dl, const ImVec2& min, float width, float height, ImU32 col)
                {
                    const float iconSize = std::min(width, height);
                    const float offsetX  = (width - iconSize) * 0.5f;
                    const float padX     = iconSize * 0.34f;
                    const float padY     = iconSize * 0.27f;
                    const ImVec2 p0(min.x + offsetX + padX, min.y + padY);
                    const ImVec2 p1(min.x + offsetX + padX, min.y + height - padY);
                    const ImVec2 p2(min.x + offsetX + iconSize - padX * 0.80f, min.y + height * 0.50f);
                    dl->AddTriangleFilled(p0, p1, p2, col);
                }
            ))
            {
                m_playState = PlayState::Playing;
                AppendTerminalLog("[Editor] Simulation state: Started");
                if (m_playStateCallback)
                {
                    m_playStateCallback(m_playState);
                }
            }

            ImGui::SameLine(0.0f, 0.0f);
            if (drawToolbarButton(
                "##ToolbarPause",
                "Pause",
                isPlaying,
                isPaused,
                ImDrawFlags_RoundCornersNone,
                IM_COL32(236, 238, 242, 255),
                IM_COL32(205, 208, 214, 255),
                [](ImDrawList* dl, const ImVec2& min, float width, float height, ImU32 col)
                {
                    const float iconSize = std::min(width, height);
                    const float barWidth = std::max(2.0f, iconSize * 0.13f);
                    const float gap      = std::max(2.0f, iconSize * 0.14f);
                    const float padY     = iconSize * 0.28f;
                    const float totalW   = barWidth * 2.0f + gap;
                    const float leftX    = min.x + (width - totalW) * 0.5f;
                    const float rightX   = leftX + barWidth + gap;
                    dl->AddRectFilled(ImVec2(leftX, min.y + padY), ImVec2(leftX + barWidth, min.y + height - padY), col, 1.0f);
                    dl->AddRectFilled(ImVec2(rightX, min.y + padY), ImVec2(rightX + barWidth, min.y + height - padY), col, 1.0f);
                }
            ))
            {
                m_playState = PlayState::Paused;
                AppendTerminalLog("[Editor] Simulation state: Paused");
                if (m_playStateCallback)
                {
                    m_playStateCallback(m_playState);
                }
            }

            ImGui::SameLine(0.0f, 0.0f);
            if (drawToolbarButton(
                "##ToolbarStop",
                "Stop",
                !isStopped,
                false,
                ImDrawFlags_RoundCornersRight,
                IM_COL32(238, 86, 92, 255),
                IM_COL32(204, 70, 76, 255),
                [](ImDrawList* dl, const ImVec2& min, float width, float height, ImU32 col)
                {
                    const float iconSize = std::min(width, height);
                    const float side     = iconSize * 0.40f;
                    const float leftX    = min.x + (width - side) * 0.5f;
                    const float topY     = min.y + (height - side) * 0.5f;
                    dl->AddRectFilled(ImVec2(leftX, topY), ImVec2(leftX + side, topY + side), col, 1.5f);
                }
            ))
            {
                m_playState = PlayState::Stopped;
                AppendTerminalLog("[Editor] Simulation state: Stopped");
                if (m_playStateCallback)
                {
                    m_playStateCallback(m_playState);
                }
            }

            // Internal segment dividers and outer container border
            const ImU32 dividerCol = IM_COL32(38, 38, 46, 255);
            drawList->AddLine(
                ImVec2(groupScreenMin.x + buttonWidth, groupScreenMin.y + 1.0f),
                ImVec2(groupScreenMin.x + buttonWidth, groupScreenMax.y - 1.0f),
                dividerCol
            );
            drawList->AddLine(
                ImVec2(groupScreenMin.x + buttonWidth * 2.0f, groupScreenMin.y + 1.0f),
                ImVec2(groupScreenMin.x + buttonWidth * 2.0f, groupScreenMax.y - 1.0f),
                dividerCol
            );
            drawList->AddRect(groupScreenMin, groupScreenMax, dividerCol, containerRound);

            // Bottom separator line between toolbar and 3D viewport
            drawList->AddLine(
                ImVec2(barMin.x, barMax.y - 1.0f),
                ImVec2(barMax.x, barMax.y - 1.0f),
                dividerCol
            );
        }
        ImGui::EndChild();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }

    void EditorWindow::DrawViewportPanel()
    {
        ImGuiWindowClass viewportWindowClass{};
        viewportWindowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
        ImGui::SetNextWindowClass(&viewportWindowClass);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        constexpr ImGuiWindowFlags viewportFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        if (ImGui::Begin(ViewportTitle, nullptr, viewportFlags))
        {
            DrawViewportToolbar();

            const ImVec2 available = ImGui::GetContentRegionAvail();
            if (available.x >= 1.0f && available.y >= 1.0f)
            {
                m_desiredViewportWidth  = static_cast<uint32_t>(available.x);
                m_desiredViewportHeight = static_cast<uint32_t>(available.y);

                if (m_hasViewportSrv)
                {
                    const UINT srvIndex = m_viewportRenderer.GetOffscreenIndex() % Renderer::SwapChain::BufferCount;
                    ImGui::Image(static_cast<ImTextureID>(m_viewportSrvGpuHandles[srvIndex].ptr), available);
                    m_isViewportHovered = ImGui::IsItemHovered();
                }
                else
                {
                    m_isViewportHovered = false;
                }
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    void EditorWindow::DrawSceneHierarchyPanel()
    {
        if (ImGui::Begin(SceneHierarchyTitle, &m_showSceneHierarchy))
        {
            ImGui::TextDisabled("No scene hierarchy bound yet.");
        }
        ImGui::End();
    }

    void EditorWindow::DrawObjectInspectorPanel()
    {
        if (ImGui::Begin(ObjectInspectorTitle, &m_showObjectInspector))
        {
            ImGui::TextDisabled("Select an object in the hierarchy to inspect its properties.");
        }
        ImGui::End();
    }

    void EditorWindow::DrawResourceBrowserPanel()
    {
        if (ImGui::Begin(ResourceBrowserTitle, &m_showResourceBrowser))
        {
            ImGui::TextDisabled("resources/");
            ImGui::BulletText("entities");
            ImGui::BulletText("environment");
            ImGui::BulletText("shared");
        }
        ImGui::End();
    }

    void EditorWindow::DrawTerminalPanel()
    {
        if (ImGui::Begin(TerminalTitle, &m_showTerminal))
        {
            if (m_monoFont)
            {
                ImGui::PushFont(m_monoFont);
            }

            const float inputFooterReserve = ImGui::GetFrameHeightWithSpacing() + 6.0f;
            if (ImGui::BeginChild("TerminalScrollRegion", ImVec2(0.0f, -inputFooterReserve), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
            {
                for (const std::string& line : m_terminalLines)
                {
                    ImGui::TextUnformatted(line.c_str());
                }
                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                {
                    ImGui::SetScrollHereY(1.0f);
                }
            }
            ImGui::EndChild();

            ImGui::Separator();
            std::array<char, 256> inputBuffer{};
            const size_t copyLen = std::min(m_terminalInputBuffer.size(), inputBuffer.size() - 1);
            std::memcpy(inputBuffer.data(), m_terminalInputBuffer.data(), copyLen);

            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputTextWithHint("##TerminalInput", "Enter command...", inputBuffer.data(), inputBuffer.size(), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                std::string command(inputBuffer.data());
                if (!command.empty())
                {
                    AppendTerminalLog("> " + command);
                    m_terminalInputBuffer.clear();
                }
            }
            else
            {
                m_terminalInputBuffer = inputBuffer.data();
            }

            if (m_monoFont)
            {
                ImGui::PopFont();
            }
        }
        ImGui::End();
    }
}
