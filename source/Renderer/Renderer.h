// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

#include "DxCheck.h"
#include "SwapChain.h"
#include "CommandContext.h"
#include "PipelineState.h"
#include "ConstantBuffer.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Mesh.h"
#include "RenderItem.h"
#include "Camera.h"

#include <d3d12.h>
#include <cstdint>
#include <vector>
#include <memory>

namespace Sandbox3D::Renderer
{
    // Constant buffer layout matching HLSL cbuffer SceneConstantBuffer
    struct SceneConstantBuffer
    {
        Maths::Mat4x4 mvp;
        Maths::Mat4x4 world;
        Maths::Vec4   lightDirection; // xyz = direction light travels, w = unused
        Maths::Vec4   lightColor;     // rgb = diffuse intensity, a = 1.0f
        Maths::Vec4   ambientColor;   // rgb = ambient intensity, a = 1.0f
    };

    using ModelViewProjectionBuffer = SceneConstantBuffer;

    // Orchestrates rendering passes, pipeline execution, and frame presentations
    class Renderer final
    {
    public:
        Renderer() = default;
        ~Renderer() = default;

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;
        Renderer(Renderer&&) noexcept = default;
        Renderer& operator=(Renderer&&) noexcept = default;

        void Initialise(
            IDXGIFactory6* factory,
            ID3D12Device* device,
            ID3D12CommandQueue* commandQueue,
            HWND hwnd,
            uint32_t width,
            uint32_t height
        );

        void Shutdown(ID3D12CommandQueue* commandQueue) noexcept;
        void OnResize(ID3D12Device* device, ID3D12CommandQueue* commandQueue, uint32_t width, uint32_t height);
        void Render(ID3D12CommandQueue* commandQueue);

        [[nodiscard]] Camera& GetCamera() noexcept { return m_camera; }
        [[nodiscard]] const Camera& GetCamera() const noexcept { return m_camera; }

        // Directional lighting management
        void SetDirectionalLight(
            const Maths::Vec3& direction,
            const Maths::Vec4& color = Maths::Vec4(1.0f, 1.0f, 1.0f, 1.0f),
            const Maths::Vec4& ambient = Maths::Vec4(0.2f, 0.2f, 0.25f, 1.0f)
        ) noexcept
        {
            const Maths::Vec3 normDir = direction.Normalised();
            m_lightDirection = Maths::Vec4(normDir.x, normDir.y, normDir.z, 0.0f);
            m_lightColor     = color;
            m_ambientColor   = ambient;
        }

        [[nodiscard]] const Maths::Vec4& GetLightDirection() const noexcept { return m_lightDirection; }
        [[nodiscard]] const Maths::Vec4& GetLightColor() const noexcept { return m_lightColor; }
        [[nodiscard]] const Maths::Vec4& GetAmbientColor() const noexcept { return m_ambientColor; }

        // Orientation gizmo management
        void SetShowGizmo(bool show) noexcept { m_showGizmo = show; }
        [[nodiscard]] bool IsGizmoVisible() const noexcept { return m_showGizmo; }
        void SetGizmoSize(float size) noexcept { m_gizmoSize = size; }
        [[nodiscard]] float GetGizmoSize() const noexcept { return m_gizmoSize; }
        void SetGizmoMargin(float margin) noexcept { m_gizmoMargin = margin; }
        [[nodiscard]] float GetGizmoMargin() const noexcept { return m_gizmoMargin; }

        // Render item management
        void AddRenderItem(RenderItem item) { m_renderItems.push_back(std::move(item)); }
        void AddRenderItem(std::shared_ptr<Mesh> mesh, const Maths::Mat4x4D& worldMatrix = Maths::Mat4x4D::Identity(), const std::string& name = {})
        {
            m_renderItems.push_back(RenderItem{ std::move(mesh), worldMatrix, true, name });
        }
        void ClearRenderItems() noexcept { m_renderItems.clear(); }
        [[nodiscard]] std::vector<RenderItem>& GetRenderItems() noexcept { return m_renderItems; }
        [[nodiscard]] const std::vector<RenderItem>& GetRenderItems() const noexcept { return m_renderItems; }

    private:
        void UpdateViewportAndScissor(uint32_t width, uint32_t height);

    private:
        SwapChain                                   m_swapChain;
        CommandContext                              m_commandContext;
        PipelineState                               m_pipelineState;
        ConstantBuffer<SceneConstantBuffer>         m_sceneConstantBuffer;
        ConstantBuffer<SceneConstantBuffer>         m_gizmoConstantBuffer;
        std::shared_ptr<Mesh>                       m_gizmoMesh;
        std::vector<RenderItem>                     m_renderItems;
        Camera                                      m_camera;

        D3D12_VIEWPORT                              m_viewport{};
        D3D12_RECT                                  m_scissorRect{};
        Maths::Rect                                 m_viewportRect{};
        Maths::Vec4                                 m_clearColor{ 0.12f, 0.14f, 0.18f, 1.0f };
        Maths::Vec4                                 m_lightDirection{ -0.577f, -0.707f, -0.408f, 0.0f };
        Maths::Vec4                                 m_lightColor{ 0.9f, 0.9f, 0.95f, 1.0f };
        Maths::Vec4                                 m_ambientColor{ 0.2f, 0.2f, 0.25f, 1.0f };
        float                                       m_gizmoSize{ 128.0f };
        float                                       m_gizmoMargin{ 16.0f };
        uint32_t                                    m_width{ 0 };
        uint32_t                                    m_height{ 0 };
        bool                                        m_showGizmo{ true };
        bool                                        m_isInitialised{ false };
    };
}

