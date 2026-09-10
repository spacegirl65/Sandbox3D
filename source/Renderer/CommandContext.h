// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "DxCheck.h"
#include "SwapChain.h"

#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>

namespace Sandbox3D::Renderer
{
    using Microsoft::WRL::ComPtr;

    // Encapsulates command allocators, command lists, and fence synchronization for frame execution
    class CommandContext final
    {
    public:
        CommandContext() = default;
        ~CommandContext();

        CommandContext(const CommandContext&) = delete;
        CommandContext& operator=(const CommandContext&) = delete;
        CommandContext(CommandContext&&) noexcept = delete;
        CommandContext& operator=(CommandContext&&) noexcept = delete;

        void Initialise(ID3D12Device* device);
        void Shutdown(ID3D12CommandQueue* commandQueue) noexcept;

        void BeginFrame(UINT frameIndex);
        void Execute(ID3D12CommandQueue* commandQueue, UINT frameIndex);
        void Flush(ID3D12CommandQueue* commandQueue);

        [[nodiscard]] ID3D12GraphicsCommandList* GetCommandList() const noexcept { return m_commandList.Get(); }

    private:
        ComPtr<ID3D12CommandAllocator>    m_commandAllocators[SwapChain::BufferCount];
        ComPtr<ID3D12GraphicsCommandList>  m_commandList;
        ComPtr<ID3D12Fence>               m_fence;
        HANDLE                            m_fenceEvent{ nullptr };
        uint64_t                          m_fenceValues[SwapChain::BufferCount]{ 0, 0 };
        uint64_t                          m_currentFenceValue{ 0 };
    };
}

