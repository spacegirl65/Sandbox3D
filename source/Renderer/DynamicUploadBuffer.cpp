// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "DynamicUploadBuffer.h"

#include <stdexcept>
#include <iostream>

namespace Sandbox3D::Renderer
{
    DynamicUploadBuffer::~DynamicUploadBuffer()
    {
        Shutdown();
    }

    DynamicUploadBuffer::DynamicUploadBuffer(DynamicUploadBuffer&& other) noexcept
        : m_device(other.m_device)
        , m_framePools(std::move(other.m_framePools))
        , m_pageSize(other.m_pageSize)
        , m_frameCount(other.m_frameCount)
        , m_currentFrameIndex(other.m_currentFrameIndex)
        , m_isInitialised(other.m_isInitialised)
    {
        other.m_device = nullptr;
        other.m_isInitialised = false;
    }

    DynamicUploadBuffer& DynamicUploadBuffer::operator=(DynamicUploadBuffer&& other) noexcept
    {
        if (this != &other)
        {
            Shutdown();

            m_device = other.m_device;
            m_framePools = std::move(other.m_framePools);
            m_pageSize = other.m_pageSize;
            m_frameCount = other.m_frameCount;
            m_currentFrameIndex = other.m_currentFrameIndex;
            m_isInitialised = other.m_isInitialised;

            other.m_device = nullptr;
            other.m_isInitialised = false;
        }
        return *this;
    }

    void DynamicUploadBuffer::Initialise(
        ID3D12Device* device,
        size_t pageSize,
        uint32_t frameCount
    )
    {
        if (!device)
        {
            throw std::invalid_argument("DynamicUploadBuffer::Initialise: Direct3D 12 device cannot be null.");
        }

        Shutdown();

        m_device = device;
        m_pageSize = (pageSize == 0) ? DefaultPageSize : pageSize;
        m_frameCount = (frameCount == 0) ? SwapChain::BufferCount : frameCount;
        m_currentFrameIndex = 0;

        m_framePools.resize(m_frameCount);
        for (uint32_t f = 0; f < m_frameCount; ++f)
        {
            m_framePools[f].pages.push_back(CreatePage(m_pageSize));
            m_framePools[f].currentPageIndex = 0;
        }

        m_isInitialised = true;
    }

    void DynamicUploadBuffer::Shutdown() noexcept
    {
        if (!m_isInitialised)
        {
            return;
        }

        m_framePools.clear();
        m_device = nullptr;
        m_currentFrameIndex = 0;
        m_isInitialised = false;
    }

    void DynamicUploadBuffer::BeginFrame(uint32_t frameIndex) noexcept
    {
        if (m_framePools.empty())
        {
            return;
        }

        m_currentFrameIndex = frameIndex % static_cast<uint32_t>(m_framePools.size());
        auto& pool = m_framePools[m_currentFrameIndex];

        pool.currentPageIndex = 0;
        for (auto& page : pool.pages)
        {
            if (page)
            {
                page->currentOffset = 0;
            }
        }
    }

    DynamicAllocation DynamicUploadBuffer::Allocate(size_t sizeInBytes, size_t alignment)
    {
        if (sizeInBytes == 0 || !m_isInitialised || !m_device)
        {
            return {};
        }

        alignment = std::max<size_t>(alignment, 1);
        auto& pool = m_framePools[m_currentFrameIndex];

        // Dedicated handling for oversized allocations exceeding the configured standard page size
        if (sizeInBytes > m_pageSize)
        {
            auto oversizedPage = CreatePage(sizeInBytes + alignment);
            const size_t alignedOffset = (oversizedPage->currentOffset + alignment - 1) & ~(alignment - 1);

            DynamicAllocation alloc;
            alloc.cpuAddress = oversizedPage->cpuBaseAddress + alignedOffset;
            alloc.gpuAddress = oversizedPage->gpuBaseAddress + alignedOffset;
            alloc.offset     = alignedOffset;
            alloc.size       = sizeInBytes;

            oversizedPage->currentOffset = alignedOffset + sizeInBytes;
            pool.pages.push_back(std::move(oversizedPage));
            return alloc;
        }

        // Iterate through existing pages in this frame pool to find one with sufficient space
        while (pool.currentPageIndex < pool.pages.size())
        {
            auto& page = pool.pages[pool.currentPageIndex];
            if (page)
            {
                const size_t alignedOffset = (page->currentOffset + alignment - 1) & ~(alignment - 1);
                if (alignedOffset + sizeInBytes <= page->sizeInBytes)
                {
                    DynamicAllocation alloc;
                    alloc.cpuAddress = page->cpuBaseAddress + alignedOffset;
                    alloc.gpuAddress = page->gpuBaseAddress + alignedOffset;
                    alloc.offset     = alignedOffset;
                    alloc.size       = sizeInBytes;

                    page->currentOffset = alignedOffset + sizeInBytes;
                    return alloc;
                }
            }

            // Current page exhausted; advance to the next page
            ++pool.currentPageIndex;
        }

        // No existing page accommodates the requested slice; allocate a new page in this frame pool
        auto newPage = CreatePage(m_pageSize);
        const size_t alignedOffset = (newPage->currentOffset + alignment - 1) & ~(alignment - 1);

        DynamicAllocation alloc;
        alloc.cpuAddress = newPage->cpuBaseAddress + alignedOffset;
        alloc.gpuAddress = newPage->gpuBaseAddress + alignedOffset;
        alloc.offset     = alignedOffset;
        alloc.size       = sizeInBytes;

        newPage->currentOffset = alignedOffset + sizeInBytes;
        pool.pages.push_back(std::move(newPage));
        pool.currentPageIndex = pool.pages.size() - 1;

        return alloc;
    }

    std::unique_ptr<DynamicUploadBuffer::Page> DynamicUploadBuffer::CreatePage(size_t sizeInBytes)
    {
        auto page = std::make_unique<Page>();
        page->sizeInBytes = sizeInBytes;
        page->currentOffset = 0;

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type                 = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask     = 1;
        heapProps.VisibleNodeMask      = 1;

        D3D12_RESOURCE_DESC bufferDesc{};
        bufferDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Alignment          = 0;
        bufferDesc.Width              = sizeInBytes;
        bufferDesc.Height             = 1;
        bufferDesc.DepthOrArraySize   = 1;
        bufferDesc.MipLevels          = 1;
        bufferDesc.Format             = DXGI_FORMAT_UNKNOWN;
        bufferDesc.SampleDesc.Count   = 1;
        bufferDesc.SampleDesc.Quality = 0;
        bufferDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufferDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;

        HR_CHECK(m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&page->resource)
        ));

        // Upload buffers remain persistently mapped into CPU address space for rapid frame writes
        D3D12_RANGE readRange{ 0, 0 };
        HR_CHECK(page->resource->Map(0, &readRange, reinterpret_cast<void**>(&page->cpuBaseAddress)));
        page->gpuBaseAddress = page->resource->GetGPUVirtualAddress();

        return page;
    }

    size_t DynamicUploadBuffer::GetCurrentFrameUsedBytes() const noexcept
    {
        if (m_framePools.empty() || m_currentFrameIndex >= m_framePools.size())
        {
            return 0;
        }

        size_t used = 0;
        for (const auto& page : m_framePools[m_currentFrameIndex].pages)
        {
            if (page)
            {
                used += page->currentOffset;
            }
        }
        return used;
    }

    size_t DynamicUploadBuffer::GetTotalAllocatedBytes() const noexcept
    {
        size_t total = 0;
        for (const auto& pool : m_framePools)
        {
            for (const auto& page : pool.pages)
            {
                if (page)
                {
                    total += page->sizeInBytes;
                }
            }
        }
        return total;
    }

    size_t DynamicUploadBuffer::GetTotalPageCount() const noexcept
    {
        size_t count = 0;
        for (const auto& pool : m_framePools)
        {
            count += pool.pages.size();
        }
        return count;
    }
}

