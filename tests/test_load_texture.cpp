// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Renderer/Texture.h"
#include "Renderer/DxCheck.h"

#include <cassert>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <iostream>
#include <wrl/client.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using namespace Sandbox3D;
using Microsoft::WRL::ComPtr;

int main()
{
    std::cout << "=== Running D3D12 Texture Subsystem Verification ===\n\n";

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

    std::cout << "[Test 1] Loading 4K BC7 Albedo texture...\n";
    ComPtr<ID3D12Resource> stagingAlbedo;
    auto albedo = Renderer::Texture::LoadFromDds(
        device.Get(),
        cmdList.Get(),
        "resources/environment/terrain/textures/uncut_grass_oilpt20_4k_albedo.dds",
        stagingAlbedo,
        "TestAlbedo"
    );
    assert(albedo != nullptr);
    assert(albedo->GetWidth() == 4096);
    assert(albedo->GetHeight() == 4096);
    assert(albedo->GetMipLevels() == 13);
    assert(albedo->GetFormat() == DXGI_FORMAT_BC7_UNORM_SRGB);
    std::cout << "  Passed: Width 4096, Height 4096, Mips 13, Format BC7_UNORM_SRGB.\n";

    std::cout << "[Test 2] Loading 4K BC5 Normal map...\n";
    ComPtr<ID3D12Resource> stagingNormal;
    auto normal = Renderer::Texture::LoadFromDds(
        device.Get(),
        cmdList.Get(),
        "resources/environment/terrain/textures/uncut_grass_oilpt20_4k_normal.dds",
        stagingNormal,
        "TestNormal"
    );
    assert(normal != nullptr);
    assert(normal->GetWidth() == 4096);
    assert(normal->GetHeight() == 4096);
    assert(normal->GetMipLevels() == 13);
    assert(normal->GetFormat() == DXGI_FORMAT_BC5_UNORM);
    std::cout << "  Passed: Width 4096, Height 4096, Mips 13, Format BC5_UNORM.\n";

    std::cout << "[Test 3] Loading 4K BC4 Roughness map...\n";
    ComPtr<ID3D12Resource> stagingRoughness;
    auto roughness = Renderer::Texture::LoadFromDds(
        device.Get(),
        cmdList.Get(),
        "resources/environment/terrain/textures/uncut_grass_oilpt20_4k_roughness.dds",
        stagingRoughness,
        "TestRoughness"
    );
    assert(roughness != nullptr);
    assert(roughness->GetWidth() == 4096);
    assert(roughness->GetHeight() == 4096);
    assert(roughness->GetMipLevels() == 13);
    assert(roughness->GetFormat() == DXGI_FORMAT_BC4_UNORM);
    std::cout << "  Passed: Width 4096, Height 4096, Mips 13, Format BC4_UNORM.\n";

    std::cout << "[Test 4] Executing upload commands on GPU...\n";
    HR_CHECK(cmdList->Close());
    ID3D12CommandList* lists[] = { cmdList.Get() };
    queue->ExecuteCommandLists(1, lists);

    ComPtr<ID3D12Fence> fence;
    HR_CHECK(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
    HR_CHECK(queue->Signal(fence.Get(), 1));
    HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    HR_CHECK(fence->SetEventOnCompletion(1, event));
    WaitForSingleObject(event, INFINITE);
    CloseHandle(event);
    std::cout << "  Passed: GPU executed texture upload commands and synchronized.\n";

    std::cout << "\nAll Texture Subsystem tests passed successfully!\n";
    return 0;
}

