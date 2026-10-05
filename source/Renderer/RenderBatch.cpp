// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "RenderBatch.h"

#include <algorithm>

namespace Sandbox3D::Renderer
{
    void RenderQueue::Build(
        std::span<const RenderItem> renderItems,
        const Maths::Vec3D& cameraPosition,
        const std::unordered_map<std::string, PipelineState>& pipelineStates,
        const PipelineState& defaultPso,
        const PipelineState* unlitPso,
        const Maths::BoundingFrustumD* cullingFrustum,
        double cullingMargin
    )
    {
        Clear();

        // Filter active visible items and classify into opaque and transparent queues
        for (const auto& item : renderItems)
        {
            if (!item.isVisible || !item.mesh)
            {
                continue;
            }

            // Perform CPU view frustum culling when a valid camera frustum is provided
            if (cullingFrustum)
            {
                const auto& sphere = item.mesh->GetBoundingSphere();
                if (sphere.radius > 0.0f)
                {
                    const Maths::BoundingSphereD worldSphere = item.GetWorldBoundingSphereD();
                    if (!cullingFrustum->Intersects(worldSphere, cullingMargin))
                    {
                        continue;
                    }
                }
                else
                {
                    const Maths::BoundingBoxD worldBox = item.GetWorldBoundingBoxD();
                    if (worldBox.GetSize().LengthSquared() > 0.0 && !cullingFrustum->Intersects(worldBox, cullingMargin))
                    {
                        continue;
                    }
                }
            }

            const std::string& shaderName = (item.material && item.material->IsUnlit())
                ? "Unlit"
                : (item.material && !item.material->GetShaderName().empty())
                    ? item.material->GetShaderName()
                    : "Standard";

            const PipelineState* pso = &defaultPso;
            if (shaderName == "Unlit" && unlitPso)
            {
                pso = unlitPso;
            }
            else
            {
                const auto it = pipelineStates.find(shaderName);
                if (it != pipelineStates.end())
                {
                    pso = &it->second;
                }
            }

            const Maths::Vec3D itemPos = item.worldMatrix.GetTranslation();
            const double depthSq = (itemPos - cameraPosition).LengthSquared();
            const bool isTransparent = item.material && item.material->IsTransparent();

            RenderQueueItem queueItem{
                .item          = &item,
                .pso           = pso,
                .material      = item.material.get(),
                .mesh          = item.mesh.get(),
                .depthSq       = depthSq,
                .isTransparent = isTransparent
            };

            const bool isUnlit = item.material && item.material->IsUnlit();
            if (isUnlit)
            {
                m_unlitItems.push_back(queueItem);
            }
            else if (isTransparent)
            {
                m_transparentItems.push_back(queueItem);
            }
            else
            {
                m_opaqueItems.push_back(queueItem);
            }

            ++m_totalItemCount;
        }

        // Sort opaque items: primary sort by pipeline state, secondary by material, tertiary by mesh, and front-to-back depth for early-Z culling
        std::sort(m_opaqueItems.begin(), m_opaqueItems.end(), [](const RenderQueueItem& a, const RenderQueueItem& b) noexcept {
            if (a.pso != b.pso)
            {
                return a.pso < b.pso;
            }
            if (a.material != b.material)
            {
                return a.material < b.material;
            }
            if (a.mesh != b.mesh)
            {
                return a.mesh < b.mesh;
            }
            return a.depthSq < b.depthSq;
        });

        // Sort transparent items: back-to-front depth for correct alpha compositing, followed by state grouping
        std::sort(m_transparentItems.begin(), m_transparentItems.end(), [](const RenderQueueItem& a, const RenderQueueItem& b) noexcept {
            if (a.depthSq != b.depthSq)
            {
                return a.depthSq > b.depthSq;
            }
            if (a.pso != b.pso)
            {
                return a.pso < b.pso;
            }
            if (a.material != b.material)
            {
                return a.material < b.material;
            }
            return a.mesh < b.mesh;
        });

        // Sort unlit items: primary sort by pipeline state, secondary by material, and tertiary by mesh
        std::sort(m_unlitItems.begin(), m_unlitItems.end(), [](const RenderQueueItem& a, const RenderQueueItem& b) noexcept {
            if (a.pso != b.pso)
            {
                return a.pso < b.pso;
            }
            if (a.material != b.material)
            {
                return a.material < b.material;
            }
            if (a.mesh != b.mesh)
            {
                return a.mesh < b.mesh;
            }
            return a.depthSq < b.depthSq;
        });

        // Group sorted items into contiguous render batches
        BuildBatches(m_opaqueItems, m_opaqueBatches);
        BuildBatches(m_transparentItems, m_transparentBatches);
        BuildBatches(m_unlitItems, m_unlitBatches);
    }

    void RenderQueue::BuildBatches(
        const std::vector<RenderQueueItem>& sortedItems,
        std::vector<RenderBatch>& outBatches
    )
    {
        outBatches.clear();

        for (const auto& queueItem : sortedItems)
        {
            if (outBatches.empty() ||
                outBatches.back().pso != queueItem.pso ||
                outBatches.back().material != queueItem.material ||
                outBatches.back().mesh != queueItem.mesh)
            {
                outBatches.emplace_back(queueItem.pso, queueItem.material, queueItem.mesh);
            }

            outBatches.back().items.push_back(queueItem.item);
        }
    }

    void RenderQueue::Clear() noexcept
    {
        m_opaqueItems.clear();
        m_transparentItems.clear();
        m_unlitItems.clear();
        m_opaqueBatches.clear();
        m_transparentBatches.clear();
        m_unlitBatches.clear();
        m_totalItemCount = 0;
    }
}

