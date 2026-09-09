// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "DxCheck.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <string>

namespace Sandbox3D
{
    using Microsoft::WRL::ComPtr;

    // GraphicsEngine acts as an OOP Facade managing Direct3D 12 and DXGI hardware subsystems
    // Lifetime and resource ownership adhere strictly to the RAII pattern.
    class GraphicsEngine final
    {
    public:
        GraphicsEngine();
        ~GraphicsEngine();

        // Enforce strict RAII ownership semantics (non-copyable, non-movable for the core engine facade)
        GraphicsEngine(const GraphicsEngine&) = delete;
        GraphicsEngine& operator=(const GraphicsEngine&) = delete;
        GraphicsEngine(GraphicsEngine&&) noexcept = delete;
        GraphicsEngine& operator=(GraphicsEngine&&) noexcept = delete;

        // Core lifecycle operations
        bool Initialise(bool enableDebugLayer = true);
        void Shutdown() noexcept;

        // Accessors
        [[nodiscard]] ID3D12Device* GetDevice() const noexcept { return m_device.Get(); }
        [[nodiscard]] ID3D12CommandQueue* GetCommandQueue() const noexcept { return m_commandQueue.Get(); }
        [[nodiscard]] IDXGIFactory6* GetFactory() const noexcept { return m_dxgiFactory.Get(); }
        [[nodiscard]] IDXGIAdapter4* GetAdapter() const noexcept { return m_selectedAdapter.Get(); }
        [[nodiscard]] const std::wstring& GetGpuDescription() const noexcept { return m_gpuDescription; }
        [[nodiscard]] size_t GetDedicatedVideoMemory() const noexcept { return m_dedicatedVideoMemory; }
        [[nodiscard]] D3D_FEATURE_LEVEL GetFeatureLevel() const noexcept { return m_featureLevel; }

    private:
        // Internal Factory Method routines for subsystem creation and configuration
        void EnableDebugSubsystem();
        void CreateDXGIFactoryInstance();
        void SelectHighPerformanceAdapter();
        void CreateD3D12DeviceInstance();
        void ConfigureInfoQueueDiagnostics();
        void CreateDirectCommandQueue();

    private:
        ComPtr<IDXGIFactory6>       m_dxgiFactory;
        ComPtr<IDXGIAdapter4>       m_selectedAdapter;
        ComPtr<ID3D12Device>        m_device;
        ComPtr<ID3D12CommandQueue>  m_commandQueue;

        std::wstring                m_gpuDescription;
        size_t                      m_dedicatedVideoMemory{ 0 };
        D3D_FEATURE_LEVEL           m_featureLevel{ D3D_FEATURE_LEVEL_12_0 };
        bool                        m_isDebugLayerEnabled{ false };
        bool                        m_isInitialised{ false };
    };
}

