// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Engine/TerrainMaterial.h"
#include "Renderer/DxCheck.h"

#include <cassert>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <iostream>
#include <vector>
#include <wrl/client.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using namespace Sandbox3D;
using Microsoft::WRL::ComPtr;

int main()
{
    std::cout << "=== Running TerrainMaterial & TerrainLayer Verification ===\n\n";

    // Test 1: Layer configuration and defaults
    std::cout << "[Test 1] Verifying default biome layer definitions...\n";
    Engine::TerrainMaterial material;
    assert(material.GetPrimaryLayers().size() == 5);
    assert(material.GetAlternativeLayers().size() == 3);
    assert(material.GetTotalLayerCount() == 8);
    assert(Engine::TerrainMaterial::RequiredDescriptorCount == 24);

    const auto primaryLayers = material.GetPrimaryLayers();
    assert(primaryLayers[0].name == "Meadow" && primaryLayers[0].assetId == "1");
    assert(primaryLayers[1].name == "MidSlope" && primaryLayers[1].assetId == "1");
    assert(primaryLayers[2].name == "Plateau" && primaryLayers[2].assetId == "2");
    assert(primaryLayers[3].name == "DryThatch" && primaryLayers[3].assetId == "1");
    assert(primaryLayers[4].name == "RockCliff" && primaryLayers[4].assetId == "1");

    const auto altLayers = material.GetAlternativeLayers();
    assert(altLayers[0].name == "AltMeadow" && altLayers[0].assetId == "2");
    assert(altLayers[1].name == "AltMidSlope" && altLayers[1].assetId == "3");
    assert(altLayers[2].name == "AltPlateau" && altLayers[2].assetId == "4");
    std::cout << "  Passed: 5 primary and 3 alternative biome layers correctly initialized.\n";

    // Test 2: D3D12 device and descriptor heap staging
    std::cout << "[Test 2] Verifying GPU descriptor heap staging and texture loading...\n";
    ComPtr<IDXGIFactory4> factory;
    HR_CHECK(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)));

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            continue;
        }
        break;
    }

    if (!adapter)
    {
        std::cout << "  Notice: Hardware GPU not available in test environment, skipping D3D12 load test.\n";
        return 0;
    }

    ComPtr<ID3D12Device> device;
    HR_CHECK(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;
    HR_CHECK(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue)));

    ComPtr<ID3D12CommandAllocator> allocator;
    HR_CHECK(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));

    ComPtr<ID3D12GraphicsCommandList> cmdList;
    HR_CHECK(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&cmdList)));

    // Allocate test SRV descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.NumDescriptors = 48;
    heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ComPtr<ID3D12DescriptorHeap> srvHeap;
    HR_CHECK(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&srvHeap)));
    const uint32_t descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    std::vector<ComPtr<ID3D12Resource>> stagingBuffers;
    const bool success = material.Load(
        device.Get(),
        cmdList.Get(),
        srvHeap->GetCPUDescriptorHandleForHeapStart(),
        descriptorSize,
        stagingBuffers
    );

    assert(success);
    assert(material.IsLoaded());
    assert(material.GetLoadedTextureCount() == 24);

    // Verify all primary layers have valid GPU textures assigned
    for (const auto& layer : material.GetPrimaryLayers())
    {
        assert(layer.albedoHeight != nullptr);
        assert(layer.normal != nullptr);
        assert(layer.orm != nullptr);
    }

    // Verify all alternative layers have valid GPU textures assigned
    for (const auto& layer : material.GetAlternativeLayers())
    {
        assert(layer.albedoHeight != nullptr);
        assert(layer.normal != nullptr);
        assert(layer.orm != nullptr);
    }

    HR_CHECK(cmdList->Close());
    ID3D12CommandList* lists[] = { cmdList.Get() };
    queue->ExecuteCommandLists(1, lists);

    ComPtr<ID3D12Fence> fence;
    HR_CHECK(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
    HR_CHECK(queue->Signal(fence.Get(), 1));
    HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (event)
    {
        HR_CHECK(fence->SetEventOnCompletion(1, event));
        WaitForSingleObject(event, INFINITE);
        CloseHandle(event);
    }

    std::cout << "  Passed: All 24 composite biome textures loaded and bound into SRV heap slots.\n";
    std::cout << "\n>>> All TerrainMaterial Verification Tests PASSED Successfully! <<<\n";
    return 0;
}

