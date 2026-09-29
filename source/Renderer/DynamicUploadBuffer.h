// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "DxCheck.h"
#include "SwapChain.h"

#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <cstddef>
#include <span>
#include <vector>
#include <memory>
#include <type_traits>
#include <algorithm>
#include <cstring>

namespace Sandbox3D::Renderer
{
    using Microsoft::WRL::ComPtr;

    // Encapsulates a CPU and GPU memory allocation slice within a dynamic upload buffer
    struct DynamicAllocation
    {
        void*                     cpuAddress{ nullptr };
        D3D12_GPU_VIRTUAL_ADDRESS gpuAddress{ 0 };
        size_t                    offset{ 0 };
        size_t                    size{ 0 };

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return cpuAddress != nullptr && gpuAddress != 0;
        }
    };

    // High-performance dynamic constant buffer and instance data ring allocator backed by D3D12 upload heaps
    class DynamicUploadBuffer final
    {
    public:
        // Default page allocation size: 2 Megabytes
        static constexpr size_t DefaultPageSize = 2 * 1024 * 1024;

        DynamicUploadBuffer() = default;
        ~DynamicUploadBuffer();

        DynamicUploadBuffer(const DynamicUploadBuffer&) = delete;
        DynamicUploadBuffer& operator=(const DynamicUploadBuffer&) = delete;
        DynamicUploadBuffer(DynamicUploadBuffer&&) noexcept;
        DynamicUploadBuffer& operator=(DynamicUploadBuffer&&) noexcept;

        // Configures the ring allocator across the specified swap chain frame count and page size
        void Initialise(
            ID3D12Device* device,
            size_t pageSize = DefaultPageSize,
            uint32_t frameCount = SwapChain::BufferCount
        );

        // Releases all allocated GPU upload pages and unmaps virtual memory addresses
        void Shutdown() noexcept;

        // Advances the allocator to the current frame index, resetting bump pointers for retired pages
        void BeginFrame(uint32_t frameIndex) noexcept;

        // Allocates a raw byte slice with required memory alignment (defaults to 256 bytes for CBVs)
        [[nodiscard]] DynamicAllocation Allocate(size_t sizeInBytes, size_t alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

        // Typed allocation convenience method for trivially copyable constant buffer structures
        template <typename T>
        [[nodiscard]] DynamicAllocation Allocate(const T& data)
        {
            static_assert(std::is_trivially_copyable_v<T>, "Constant buffer payload type must be trivially copyable.");
            DynamicAllocation alloc = Allocate(sizeof(T), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
            if (alloc.IsValid())
            {
                std::memcpy(alloc.cpuAddress, &data, sizeof(T));
            }
            return alloc;
        }

        // Typed allocation convenience method for contiguous arrays and instance data streams
        template <typename T>
        [[nodiscard]] DynamicAllocation Allocate(std::span<const T> data, size_t alignment = alignof(T))
        {
            static_assert(std::is_trivially_copyable_v<T>, "Array payload type must be trivially copyable.");
            const size_t byteSize = data.size_bytes();
            if (byteSize == 0)
            {
                return {};
            }
            DynamicAllocation alloc = Allocate(byteSize, alignment);
            if (alloc.IsValid())
            {
                std::memcpy(alloc.cpuAddress, data.data(), byteSize);
            }
            return alloc;
        }

        // Diagnostic queries and memory telemetry
        [[nodiscard]] bool IsInitialised() const noexcept { return m_isInitialised; }
        [[nodiscard]] size_t GetPageSize() const noexcept { return m_pageSize; }
        [[nodiscard]] uint32_t GetFrameCount() const noexcept { return m_frameCount; }
        [[nodiscard]] size_t GetCurrentFrameUsedBytes() const noexcept;
        [[nodiscard]] size_t GetTotalAllocatedBytes() const noexcept;
        [[nodiscard]] size_t GetTotalPageCount() const noexcept;

    private:
        struct Page
        {
            ComPtr<ID3D12Resource>    resource;
            uint8_t*                  cpuBaseAddress{ nullptr };
            D3D12_GPU_VIRTUAL_ADDRESS gpuBaseAddress{ 0 };
            size_t                    sizeInBytes{ 0 };
            size_t                    currentOffset{ 0 };

            ~Page()
            {
                if (resource && cpuBaseAddress)
                {
                    resource->Unmap(0, nullptr);
                    cpuBaseAddress = nullptr;
                }
            }
        };

        struct FramePool
        {
            std::vector<std::unique_ptr<Page>> pages;
            size_t                             currentPageIndex{ 0 };
        };

        [[nodiscard]] std::unique_ptr<Page> CreatePage(size_t sizeInBytes);

    private:
        ID3D12Device*            m_device{ nullptr };
        std::vector<FramePool>   m_framePools{};
        size_t                   m_pageSize{ DefaultPageSize };
        uint32_t                 m_frameCount{ SwapChain::BufferCount };
        uint32_t                 m_currentFrameIndex{ 0 };
        bool                     m_isInitialised{ false };
    };

    // Alias for explicit domain nomenclature
    using ConstantBufferRingAllocator = DynamicUploadBuffer;
}

