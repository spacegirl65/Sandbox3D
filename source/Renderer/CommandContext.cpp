// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "CommandContext.h"

#include <stdexcept>

namespace Sandbox3D::Renderer
{
    CommandContext::~CommandContext()
    {
        if (m_fenceEvent)
        {
            CloseHandle(m_fenceEvent);
            m_fenceEvent = nullptr;
        }
    }

    void CommandContext::Initialise(ID3D12Device* device)
    {
        for (UINT i = 0; i < SwapChain::BufferCount; ++i)
        {
            HR_CHECK(device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&m_commandAllocators[i])
            ));
        }

        HR_CHECK(device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            m_commandAllocators[0].Get(),
            nullptr,
            IID_PPV_ARGS(&m_commandList)
        ));

        // Command lists are created in recording state; close initially
        HR_CHECK(m_commandList->Close());

        HR_CHECK(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_currentFenceValue = 0;

        m_fenceEvent = CreateEventExW(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
        if (!m_fenceEvent)
        {
            throw std::runtime_error("Failed to create fence event handle.");
        }
    }

    void CommandContext::Shutdown(ID3D12CommandQueue* commandQueue) noexcept
    {
        if (commandQueue && m_fence)
        {
            Flush(commandQueue);
        }

        if (m_fenceEvent)
        {
            CloseHandle(m_fenceEvent);
            m_fenceEvent = nullptr;
        }

        m_fence.Reset();
        m_commandList.Reset();
        for (UINT i = 0; i < SwapChain::BufferCount; ++i)
        {
            m_commandAllocators[i].Reset();
        }
    }

    void CommandContext::BeginFrame(UINT frameIndex)
    {
        // Wait until the GPU has completed execution for this frame buffer
        if (m_fence->GetCompletedValue() < m_fenceValues[frameIndex])
        {
            HR_CHECK(m_fence->SetEventOnCompletion(m_fenceValues[frameIndex], m_fenceEvent));
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }

        HR_CHECK(m_commandAllocators[frameIndex]->Reset());
        HR_CHECK(m_commandList->Reset(m_commandAllocators[frameIndex].Get(), nullptr));
    }

    void CommandContext::Execute(ID3D12CommandQueue* commandQueue, UINT frameIndex)
    {
        HR_CHECK(m_commandList->Close());

        ID3D12CommandList* const commandLists[] = { m_commandList.Get() };
        commandQueue->ExecuteCommandLists(1, commandLists);

        ++m_currentFenceValue;
        HR_CHECK(commandQueue->Signal(m_fence.Get(), m_currentFenceValue));
        m_fenceValues[frameIndex] = m_currentFenceValue;
    }

    void CommandContext::Flush(ID3D12CommandQueue* commandQueue)
    {
        ++m_currentFenceValue;
        HR_CHECK(commandQueue->Signal(m_fence.Get(), m_currentFenceValue));
        HR_CHECK(m_fence->SetEventOnCompletion(m_currentFenceValue, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}

