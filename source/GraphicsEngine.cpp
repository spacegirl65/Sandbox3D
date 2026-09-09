// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "GraphicsEngine.h"

#include <iostream>
#include <vector>

namespace Sandbox3D
{
    GraphicsEngine::GraphicsEngine() = default;

    GraphicsEngine::~GraphicsEngine()
    {
        Shutdown();
    }

    bool GraphicsEngine::Initialise(bool enableDebugLayer)
    {
        if (m_isInitialised)
        {
            return true;
        }

        m_isDebugLayerEnabled = enableDebugLayer;

        if (m_isDebugLayerEnabled)
        {
            EnableDebugSubsystem();
        }

        CreateDXGIFactoryInstance();
        SelectHighPerformanceAdapter();
        CreateD3D12DeviceInstance();

        if (m_isDebugLayerEnabled)
        {
            ConfigureInfoQueueDiagnostics();
        }

        CreateDirectCommandQueue();

        m_isInitialised = true;
        return true;
    }

    void GraphicsEngine::Shutdown() noexcept
    {
        if (!m_isInitialised)
        {
            return;
        }

        // Release hardware resources in reverse order of initialisation
        m_commandQueue.Reset();
        m_device.Reset();
        m_selectedAdapter.Reset();
        m_dxgiFactory.Reset();

        m_isInitialised = false;
    }

    void GraphicsEngine::EnableDebugSubsystem()
    {
#if defined(_DEBUG)
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            // Attempt to query ID3D12Debug1 for optional GPU-based validation
            ComPtr<ID3D12Debug1> debugController1;
            if (SUCCEEDED(debugController.As(&debugController1)))
            {
                debugController1->SetEnableGPUBasedValidation(FALSE); // Keep disabled by default for rapid launch
            }

            OutputDebugStringW(L"[GraphicsEngine] Direct3D 12 Debug Validation Layer enabled.\n");
        }
#endif
    }

    void GraphicsEngine::CreateDXGIFactoryInstance()
    {
        UINT createFactoryFlags = 0;
#if defined(_DEBUG)
        if (m_isDebugLayerEnabled)
        {
            createFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
#endif
        HR_CHECK(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&m_dxgiFactory)));
    }

    void GraphicsEngine::SelectHighPerformanceAdapter()
    {
        ComPtr<IDXGIAdapter1> candidateAdapter;
        ComPtr<IDXGIAdapter4> bestDiscreteAdapter;
        ComPtr<IDXGIAdapter4> fallbackAdapter;

        size_t maxDedicatedVideoMemory = 0;

        // Query the DXGI Factory prioritizing high-performance hardware adapters
        for (UINT adapterIndex = 0;
             m_dxgiFactory->EnumAdapterByGpuPreference(
                 adapterIndex,
                 DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                 IID_PPV_ARGS(&candidateAdapter)) != DXGI_ERROR_NOT_FOUND;
             ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            HR_CHECK(candidateAdapter->GetDesc1(&desc));

            // Skip software rasterisers (WARP and Microsoft Basic Render Driver)
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                continue;
            }

            // Verify device creation compatibility at D3D_FEATURE_LEVEL_12_0
            if (FAILED(D3D12CreateDevice(candidateAdapter.Get(), D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)))
            {
                continue;
            }

            ComPtr<IDXGIAdapter4> adapter4;
            HR_CHECK(candidateAdapter.As(&adapter4));

            // Distinguish discrete GPU by checking for dedicated video memory
            const bool isDiscrete = (desc.DedicatedVideoMemory > 0);

            if (isDiscrete)
            {
                if (desc.DedicatedVideoMemory > maxDedicatedVideoMemory)
                {
                    maxDedicatedVideoMemory = desc.DedicatedVideoMemory;
                    bestDiscreteAdapter = adapter4;
                }
            }
            else if (!fallbackAdapter)
            {
                fallbackAdapter = adapter4;
            }
        }

        // Enforce rule: discrete GPU is required unless no other hardware option is present
        if (bestDiscreteAdapter)
        {
            m_selectedAdapter = bestDiscreteAdapter;
        }
        else if (fallbackAdapter)
        {
            m_selectedAdapter = fallbackAdapter;
            OutputDebugStringW(L"[GraphicsEngine] WARNING: Discrete GPU not found. Falling back to integrated adapter.\n");
        }
        else
        {
            throw std::runtime_error("DirectX 12 initialisation failure: No compatible Direct3D 12 adapter discovered.");
        }

        // Cache chosen adapter properties
        DXGI_ADAPTER_DESC3 selectedDesc;
        HR_CHECK(m_selectedAdapter->GetDesc3(&selectedDesc));

        m_gpuDescription = selectedDesc.Description;
        m_dedicatedVideoMemory = selectedDesc.DedicatedVideoMemory;

        // Format and print the selected GPU details to both the debugger console and standard console
        const double dedicatedMemoryMB = static_cast<double>(m_dedicatedVideoMemory) / (1024.0 * 1024.0);
        const double dedicatedMemoryGB = dedicatedMemoryMB / 1024.0;

        const std::wstring logMessage = std::format(
            L"\n============================================================\n"
            L"[GraphicsEngine] Selected Hardware Adapter: {}\n"
            L"[GraphicsEngine] Dedicated Video Memory:    {:.2f} MB ({:.2f} GB)\n"
            L"[GraphicsEngine] Vendor ID: 0x{:04X} | Device ID: 0x{:04X}\n"
            L"============================================================\n\n",
            m_gpuDescription,
            dedicatedMemoryMB,
            dedicatedMemoryGB,
            selectedDesc.VendorId,
            selectedDesc.DeviceId
        );

        OutputDebugStringW(logMessage.c_str());
        std::wcout << logMessage;
    }

    void GraphicsEngine::CreateD3D12DeviceInstance()
    {
        // Enforce Direct3D 12 feature levels, seeking 12_2 (D3D12 Ultimate), falling back to 12_1 or 12_0
        constexpr D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_12_2,
            D3D_FEATURE_LEVEL_12_1,
            D3D_FEATURE_LEVEL_12_0
        };

        HRESULT hr = E_FAIL;
        for (const auto level : featureLevels)
        {
            hr = D3D12CreateDevice(m_selectedAdapter.Get(), level, IID_PPV_ARGS(&m_device));
            if (SUCCEEDED(hr))
            {
                m_featureLevel = level;
                break;
            }
        }

        HR_CHECK(hr);

        const std::wstring levelLog = std::format(
            L"[GraphicsEngine] Created ID3D12Device successfully (Feature Level: 0x{:04X}).\n",
            static_cast<unsigned int>(m_featureLevel)
        );
        OutputDebugStringW(levelLog.c_str());
        std::wcout << levelLog;
    }

    void GraphicsEngine::ConfigureInfoQueueDiagnostics()
    {
#if defined(_DEBUG)
        ComPtr<ID3D12InfoQueue> infoQueue;
        if (SUCCEEDED(m_device.As(&infoQueue)))
        {
            HR_CHECK(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
            HR_CHECK(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
            HR_CHECK(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, FALSE));

            // Suppress benign informational messages
            D3D12_MESSAGE_SEVERITY suppressSeverities[] = {
                D3D12_MESSAGE_SEVERITY_INFO
            };

            D3D12_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumSeverities = _countof(suppressSeverities);
            filter.DenyList.pSeverityList = suppressSeverities;

            HR_CHECK(infoQueue->PushStorageFilter(&filter));
            OutputDebugStringW(L"[GraphicsEngine] ID3D12InfoQueue diagnostics configured with error break points.\n");
        }
#endif
    }

    void GraphicsEngine::CreateDirectCommandQueue()
    {
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        queueDesc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.NodeMask = 0;

        HR_CHECK(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

        OutputDebugStringW(L"[GraphicsEngine] Direct Command Queue initialised successfully.\n");
        std::wcout << L"[GraphicsEngine] Direct Command Queue initialised successfully.\n";
    }
}

