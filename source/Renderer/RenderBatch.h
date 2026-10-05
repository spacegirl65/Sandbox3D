// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "RenderItem.h"
#include "PipelineState.h"
#include "Material.h"
#include "Mesh.h"
#include "Maths/Maths.h"

#include <vector>
#include <span>
#include <unordered_map>
#include <string>

namespace Sandbox3D::Renderer
{
    // Represents a single item in the sorted render queue
    struct RenderQueueItem
    {
        const RenderItem*    item{ nullptr };
        const PipelineState* pso{ nullptr };
        const Material*      material{ nullptr };
        const Mesh*          mesh{ nullptr };
        double               depthSq{ 0.0 };
        bool                 isTransparent{ false };
    };

    // Represents a contiguous batch of draw items sharing pipeline state, material, and mesh geometry
    struct RenderBatch
    {
        const PipelineState*           pso{ nullptr };
        const Material*                material{ nullptr };
        const Mesh*                    mesh{ nullptr };
        std::vector<const RenderItem*> items{};

        RenderBatch() = default;
        RenderBatch(const PipelineState* inPso, const Material* inMaterial, const Mesh* inMesh)
            : pso(inPso), material(inMaterial), mesh(inMesh)
        {
        }
    };

    // Collects, filters, sorts, and batches render items into state-minimised draw groups
    class RenderQueue final
    {
    public:
        RenderQueue() = default;
        ~RenderQueue() = default;

        RenderQueue(const RenderQueue&) = delete;
        RenderQueue& operator=(const RenderQueue&) = delete;
        RenderQueue(RenderQueue&&) noexcept = default;
        RenderQueue& operator=(RenderQueue&&) noexcept = default;

        // Populates, sorts, and batches active render items against the camera viewpoint
        void Build(
            std::span<const RenderItem> renderItems,
            const Maths::Vec3D& cameraPosition,
            const std::unordered_map<std::string, PipelineState>& pipelineStates,
            const PipelineState& defaultPso,
            const PipelineState* unlitPso = nullptr,
            const Maths::BoundingFrustumD* cullingFrustum = nullptr,
            double cullingMargin = 25.0
        );

        [[nodiscard]] std::span<const RenderBatch> GetOpaqueBatches() const noexcept { return m_opaqueBatches; }
        [[nodiscard]] std::span<const RenderBatch> GetTransparentBatches() const noexcept { return m_transparentBatches; }
        [[nodiscard]] std::span<const RenderBatch> GetUnlitBatches() const noexcept { return m_unlitBatches; }

        [[nodiscard]] size_t GetTotalItemCount() const noexcept { return m_totalItemCount; }
        [[nodiscard]] size_t GetTotalBatchCount() const noexcept { return m_opaqueBatches.size() + m_transparentBatches.size() + m_unlitBatches.size(); }

        void Clear() noexcept;

    private:
        void BuildBatches(
            const std::vector<RenderQueueItem>& sortedItems,
            std::vector<RenderBatch>& outBatches
        );

    private:
        std::vector<RenderQueueItem> m_opaqueItems{};
        std::vector<RenderQueueItem> m_transparentItems{};
        std::vector<RenderQueueItem> m_unlitItems{};
        std::vector<RenderBatch>     m_opaqueBatches{};
        std::vector<RenderBatch>     m_transparentBatches{};
        std::vector<RenderBatch>     m_unlitBatches{};
        size_t                       m_totalItemCount{ 0 };
    };
}

