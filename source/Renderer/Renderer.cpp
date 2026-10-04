// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Renderer.h"

#include <iostream>
#include <filesystem>
#include <algorithm>

namespace Sandbox3D::Renderer
{
    // Embedded fallback HLSL source ensuring zero external file path launch dependencies
    static constexpr const char* s_embeddedSceneBuffers = R"(
        struct LightData
        {
            float4 position;    // xyz = world position, w = range
            float4 direction;   // xyz = normalized direction, w = type (0=Dir, 1=Point, 2=Spot)
            float4 color;       // rgb = light color, w = intensity
            float4 attenuation; // x = constant, y = linear, z = quadratic, w = inner/outer spot cosine
        };

        struct InstanceData
        {
            row_major float4x4 mvp;
            row_major float4x4 world;
            float4             colorTint;
        };

        cbuffer SceneConstantBuffer : register(b0)
        {
            row_major float4x4 g_mvp;
            row_major float4x4 g_world;
            float4             g_ambientColor;
            float4             g_fogColor;
            float4             g_fogParams;
            uint               g_lightCount;
            uint               g_isInstanced;
            uint               g_objectLightChannels;
            uint               g_padding;
            LightData          g_lights[16];
        };

        StructuredBuffer<InstanceData> g_instances : register(t0);
    )";

    static constexpr const char* s_embeddedVertexShaderStage = R"(
        struct VertexInput
        {
            float3 position : POSITION;
            float3 normal   : NORMAL;
            float4 color    : COLOR;
        };

        struct VertexOutput
        {
            float4 position      : SV_POSITION;
            float3 worldNormal   : NORMAL;
            float4 color         : COLOR;
            float3 worldPosition : TEXCOORD0;
        };

        VertexOutput VSMain(VertexInput input, uint instanceId : SV_InstanceID)
        {
            VertexOutput output;

            if (g_isInstanced != 0)
            {
                InstanceData inst    = g_instances[instanceId];
                output.position      = mul(float4(input.position, 1.0f), inst.mvp);
                output.worldNormal   = normalize(mul(float4(input.normal, 0.0f), inst.world).xyz);
                output.worldPosition = mul(float4(input.position, 1.0f), inst.world).xyz;
                output.color         = input.color * inst.colorTint;
            }
            else
            {
                output.position      = mul(float4(input.position, 1.0f), g_mvp);
                output.worldNormal   = normalize(mul(float4(input.normal, 0.0f), g_world).xyz);
                output.worldPosition = mul(float4(input.position, 1.0f), g_world).xyz;
                output.color         = input.color;
            }

            return output;
        }
    )";

    static constexpr const char* s_embeddedTerrainPixelStage = R"(
        cbuffer TerrainBuffer : register(b1)
        {
            uint g_enableAlternativeTextures;
        };

        float4 PSMain(VertexOutput input) : SV_TARGET
        {
            const float3 N = normalize(input.worldNormal);
            const float cameraDist = length(input.worldPosition);
            const float3 V = (cameraDist > 0.001f) ? (-input.worldPosition / cameraDist) : float3(0.0f, 1.0f, 0.0f);

            // Material response determination from vertex colour, saturation, and normal slope
            const float colorSaturation = max(max(input.color.r, input.color.g), input.color.b) -
                                          min(min(input.color.r, input.color.g), input.color.b);
            const float luminance = dot(input.color.rgb, float3(0.299f, 0.587f, 0.114f));

            // Altitudinal zone evaluation across procedural and LiDAR height ranges
            const float altNorm = (input.terrainPosition.y > 100.0f)
                ? saturate((input.terrainPosition.y - 100.0f) / 450.0f)
                : saturate((input.terrainPosition.y + 4.0f) / 93.6f);

            const float valleyFade = smoothstep(0.18f, 0.26f, altNorm);

            // Continuous slope and desaturation factors preventing sharp specular threshold facets
            const float rockSlopeMinNy = 0.70f;
            const float rockSlopeMaxNy = 0.85f;
            const float slopeRockFactor = 1.0f - smoothstep(rockSlopeMinNy, rockSlopeMaxNy, N.y);

            const float desatMin = 0.04f;
            const float desatMax = 0.12f;
            const float desatFactor = 1.0f - smoothstep(desatMin, desatMax, colorSaturation);

            const float rockGreenMin = 0.50f;
            const float rockGreenMax = 0.65f;
            const float lowGreenFactor = 1.0f - smoothstep(rockGreenMin, rockGreenMax, input.color.g);

            const float satRockFactor = desatFactor * lowGreenFactor;

            // Crevice bed rock exposure: higher fell furrows strongly expose bare crag rock, while lower valley swales blend with pasture
            const float creviceRockExposure = input.color.a * lerp(0.35f, 1.0f, valleyFade);
            const float rockFactor = saturate(max(max(slopeRockFactor, satRockFactor), creviceRockExposure));

            const float peatLumMin = 0.18f;
            const float peatLumMax = 0.24f;
            const float lowLuminance = 1.0f - smoothstep(peatLumMin, peatLumMax, luminance);

            const float peatGreenMin = 0.20f;
            const float peatGreenMax = 0.26f;
            const float lowPeatGreen = 1.0f - smoothstep(peatGreenMin, peatGreenMax, input.color.g);
            const float peatFactor = lowLuminance * lowPeatGreen * (1.0f - rockFactor);

            // Physically grounded specular characteristics for upland terrain materials
            const float turfSpecPower = 16.0f;
            const float turfSpecIntensity = 0.02f;
            const float rockSpecPower = 28.0f;
            const float rockSpecIntensity = 0.16f;
            const float peatSpecPower = 14.0f;
            const float peatSpecIntensity = 0.10f;

            float specPower = lerp(turfSpecPower, rockSpecPower, rockFactor);
            specPower = lerp(specPower, peatSpecPower, peatFactor);

            float specIntensity = lerp(turfSpecIntensity, rockSpecIntensity, rockFactor);
            specIntensity = lerp(specIntensity, peatSpecIntensity, peatFactor);

            float3 ambient = g_ambientColor.rgb;
            float3 totalDiffuse = float3(0.0f, 0.0f, 0.0f);
            float3 totalSpecular = float3(0.0f, 0.0f, 0.0f);

            const uint activeLightCount = min(g_lightCount, 16u);

            for (uint i = 0; i < activeLightCount; ++i)
            {
                LightData light = g_lights[i];
                uint packed = asuint(light.direction.w);
                uint lightType = packed & 0xFu;
                uint lightChannels = packed >> 4u;

                if ((lightChannels & g_objectLightChannels) == 0u)
                {
                    continue;
                }

                float intensity = light.color.w;
                float3 lightRgb = light.color.rgb * intensity;

                if (lightType == 0u) // Directional Light
                {
                    float3 L = normalize(-light.direction.xyz);
                    float nDotL = max(dot(N, L), 0.0f);
                    totalDiffuse += lightRgb * nDotL;

                    if (nDotL > 0.0f)
                    {
                        float3 H = normalize(L + V);
                        float nDotH = max(dot(N, H), 0.0f);
                        totalSpecular += lightRgb * (pow(nDotH, specPower) * specIntensity);
                    }
                }
                else if (lightType == 1u) // Point Light
                {
                    float3 toLight = light.position.xyz - input.worldPosition;
                    float dist = length(toLight);
                    float range = max(light.position.w, 0.001f);

                    if (dist < range)
                    {
                        float3 L = toLight / dist;
                        float nDotL = max(dot(N, L), 0.0f);

                        // Quadratic attenuation with smooth quadratic range windowing
                        float att = 1.0f / (light.attenuation.x + light.attenuation.y * dist + light.attenuation.z * dist * dist);
                        float falloff = saturate(1.0f - (dist / range));
                        falloff *= falloff;

                        float3 radiance = lightRgb * (att * falloff);
                        totalDiffuse += radiance * nDotL;

                        if (nDotL > 0.0f)
                        {
                            float3 H = normalize(L + V);
                            float nDotH = max(dot(N, H), 0.0f);
                            totalSpecular += radiance * (pow(nDotH, specPower) * specIntensity);
                        }
                    }
                }
                else if (lightType == 2u) // Spot Light
                {
                    float3 toLight = light.position.xyz - input.worldPosition;
                    float dist = length(toLight);
                    float range = max(light.position.w, 0.001f);

                    if (dist < range)
                    {
                        float3 L = toLight / dist;
                        float nDotL = max(dot(N, L), 0.0f);

                        // Spot cone factor (attenuation.z = inner cone cos, attenuation.w = outer cone cos)
                        float cosAngle = dot(-L, normalize(light.direction.xyz));
                        float innerCos = light.attenuation.z;
                        float outerCos = light.attenuation.w;
                        float spotFactor = saturate((cosAngle - outerCos) / max(innerCos - outerCos, 0.0001f));

                        float att = 1.0f / (light.attenuation.x + light.attenuation.y * dist + light.attenuation.z * dist * dist);
                        float falloff = saturate(1.0f - (dist / range));
                        falloff *= falloff;

                        float3 radiance = lightRgb * (att * falloff * spotFactor);
                        totalDiffuse += radiance * nDotL;

                        if (nDotL > 0.0f)
                        {
                            float3 H = normalize(L + V);
                            float nDotH = max(dot(N, H), 0.0f);
                            totalSpecular += radiance * (pow(nDotH, specPower) * specIntensity);
                        }
                    }
                }
            }

            float3 shadedColor = input.color.rgb * (ambient + totalDiffuse) + totalSpecular;

            // Atmospheric perspective (aerial distance fog)
            if (g_fogColor.a > 0.0f)
            {
                const float fogExtent = max(cameraDist - g_fogParams.x, 0.0f);
                const float opticalDepth = fogExtent * g_fogParams.z;
                // Exponential squared optical depth formulation
                const float fogFactor = saturate((1.0f - exp(-opticalDepth * opticalDepth)) * g_fogColor.a);
                shadedColor = lerp(shadedColor, g_fogColor.rgb, fogFactor);
            }

            return float4(shadedColor, 1.0f);
        }
    )";

    static constexpr const char* s_embeddedStandardPixelStage = R"(
        static const float PI = 3.14159265359f;

        // Trowbridge-Reitz GGX normal distribution function (NDF)
        float DistributionGGX(float3 N, float3 H, float roughness)
        {
            float a = roughness * roughness;
            float a2 = a * a;
            float nDotH = max(dot(N, H), 0.0f);
            float nDotH2 = nDotH * nDotH;

            float denom = (nDotH2 * (a2 - 1.0f) + 1.0f);
            denom = PI * denom * denom;

            return a2 / max(denom, 0.0000001f);
        }

        // Schlick-GGX geometric shadowing and masking function for single direction
        float GeometrySchlickGGX(float nDotV, float roughness)
        {
            float r = (roughness + 1.0f);
            float k = (r * r) / 8.0f;

            float denom = nDotV * (1.0f - k) + k;
            return nDotV / max(denom, 0.0000001f);
        }

        // Smith model combining geometric shadowing and masking for view and light vectors
        float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
        {
            float nDotV = max(dot(N, V), 0.0f);
            float nDotL = max(dot(N, L), 0.0f);
            float ggx2 = GeometrySchlickGGX(nDotV, roughness);
            float ggx1 = GeometrySchlickGGX(nDotL, roughness);

            return ggx1 * ggx2;
        }

        // Fresnel-Schlick approximation evaluating surface reflectance at glancing angles
        float3 FresnelSchlick(float cosTheta, float3 F0)
        {
            return F0 + (1.0f - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
        }

        float4 PSMain(VertexOutput input) : SV_TARGET
        {
            const float3 N = normalize(input.worldNormal);
            const float cameraDist = length(input.worldPosition);
            const float3 V = (cameraDist > 0.001f) ? (-input.worldPosition / cameraDist) : float3(0.0f, 1.0f, 0.0f);
            const float nDotV = max(dot(N, V), 0.0001f);

            // Physically based metallic white material characteristics
            const float3 albedo = saturate(input.color.rgb * float3(0.98f, 0.98f, 1.0f));
            const float metallic = 0.75f;
            const float roughness = 0.38f;

            // Specular reflectance at normal incidence: dielectrics default to 0.04, conductors tinted by albedo
            const float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);

            // Ambient lighting: energy-conserving diffuse alongside bright outdoor environment reflection
            const float3 ambientFresnel = FresnelSchlick(nDotV, F0);
            const float3 kSAmbient = ambientFresnel;
            const float3 kDAmbient = (float3(1.0f, 1.0f, 1.0f) - kSAmbient) * (1.0f - metallic);

            // Hemispherical environment radiance reflecting bright sky dome and ground bounce
            const float3 R = reflect(-V, N);
            const float envHemisphere = saturate(R.y * 0.5f + 0.5f);
            const float3 groundAmbient = float3(0.70f, 0.76f, 0.70f);
            const float3 skyAmbient    = float3(0.94f, 0.97f, 1.00f);
            const float3 envRadiance   = lerp(groundAmbient, skyAmbient, envHemisphere);
            const float3 ambientSpecular = ambientFresnel * envRadiance * (1.0f - roughness * 0.3f);
            const float3 ambientDiffuse  = (kDAmbient + 0.16f) * albedo * g_ambientColor.rgb * 1.5f;

            float3 totalDirect = float3(0.0f, 0.0f, 0.0f);

            const uint activeLightCount = min(g_lightCount, 16u);

            for (uint i = 0; i < activeLightCount; ++i)
            {
                LightData light = g_lights[i];
                uint packed = asuint(light.direction.w);
                uint lightType = packed & 0xFu;
                uint lightChannels = packed >> 4u;

                if ((lightChannels & g_objectLightChannels) == 0u)
                {
                    continue;
                }

                float intensity = light.color.w;
                float3 lightRgb = light.color.rgb * intensity;

                float3 L = float3(0.0f, 0.0f, 0.0f);
                float3 radiance = float3(0.0f, 0.0f, 0.0f);

                if (lightType == 0u) // Directional Light
                {
                    L = normalize(-light.direction.xyz);
                    radiance = lightRgb;
                }
                else if (lightType == 1u) // Point Light
                {
                    float3 toLight = light.position.xyz - input.worldPosition;
                    float dist     = length(toLight);
                    float range    = max(light.position.w, 0.001f);

                    if (dist < range)
                    {
                        L = toLight / dist;

                        // Quadratic attenuation with smooth quadratic range windowing
                        float att     = 1.0f / (light.attenuation.x + light.attenuation.y * dist + light.attenuation.z * dist * dist);
                        float falloff = saturate(1.0f - (dist / range));
                        falloff *= falloff;

                        radiance = lightRgb * (att * falloff);
                    }
                }
                else if (lightType == 2u) // Spot Light
                {
                    float3 toLight = light.position.xyz - input.worldPosition;
                    float dist     = length(toLight);
                    float range    = max(light.position.w, 0.001f);

                    if (dist < range)
                    {
                        L = toLight / dist;

                        // Spot cone factor (attenuation.z = inner cone cos, attenuation.w = outer cone cos)
                        float cosAngle   = dot(-L, normalize(light.direction.xyz));
                        float innerCos   = light.attenuation.z;
                        float outerCos   = light.attenuation.w;
                        float spotFactor = saturate((cosAngle - outerCos) / max(innerCos - outerCos, 0.0001f));

                        float att     = 1.0f / (light.attenuation.x + light.attenuation.y * dist + light.attenuation.z * dist * dist);
                        float falloff = saturate(1.0f - (dist / range));
                        falloff *= falloff;

                        radiance = lightRgb * (att * falloff * spotFactor);
                    }
                }

                float nDotL = max(dot(N, L), 0.0f);
                if (nDotL > 0.0f && any(radiance > 0.0f))
                {
                    float3 H = normalize(L + V);
                    float hDotV = max(dot(H, V), 0.0f);

                    // Cook-Torrance microfacet specular BRDF evaluation
                    float  NDF = DistributionGGX(N, H, roughness);
                    float  G   = GeometrySmith(N, V, L, roughness);
                    float3 F   = FresnelSchlick(hDotV, F0);

                    float3 numerator    = NDF * G * F;
                    float  denominator  = 4.0f * nDotV * nDotL + 0.0001f;
                    float3 specularTerm = numerator / denominator;

                    // Energy conservation: diffuse reflections alongside pearlescent white body
                    float3 kS = F;
                    float3 kD = (float3(1.0f, 1.0f, 1.0f) - kS) * (1.0f - metallic);

                    float3 directDiffuse  = (kD + 0.16f) * albedo;
                    float3 directLighting = (directDiffuse + specularTerm) * radiance * nDotL;

                    totalDirect += directLighting;
                }
            }

            float3 shadedColor = ambientDiffuse + ambientSpecular + totalDirect;

            // Atmospheric perspective (aerial distance fog)
            if (g_fogColor.a > 0.0f)
            {
                const float fogExtent    = max(cameraDist - g_fogParams.x, 0.0f);
                const float opticalDepth = fogExtent * g_fogParams.z;
                // Exponential squared optical depth formulation
                const float fogFactor    = saturate((1.0f - exp(-opticalDepth * opticalDepth)) * g_fogColor.a);
                shadedColor = lerp(shadedColor, g_fogColor.rgb, fogFactor);
            }

            return float4(shadedColor, input.color.a);
        }
    )";

    static constexpr const char* s_embeddedUnlitPixelStage = R"(
        float4 PSMain(VertexOutput input) : SV_TARGET
        {
            return input.color;
        }
    )";

    void Renderer::Initialise(
        IDXGIFactory6* factory,
        ID3D12Device* device,
        ID3D12CommandQueue* commandQueue,
        HWND hwnd,
        uint32_t width,
        uint32_t height,
        const std::wstring& gpuDescription
    )
    {
        m_width  = width;
        m_height = height;

        if (!gpuDescription.empty())
        {
            const int sizeNeeded = WideCharToMultiByte(
                CP_UTF8,
                0,
                gpuDescription.data(),
                static_cast<int>(gpuDescription.size()),
                nullptr,
                0,
                nullptr,
                nullptr
            );
            if (sizeNeeded > 0)
            {
                m_gpuName.resize(sizeNeeded);
                WideCharToMultiByte(
                    CP_UTF8,
                    0,
                    gpuDescription.data(),
                    static_cast<int>(gpuDescription.size()),
                    m_gpuName.data(),
                    sizeNeeded,
                    nullptr,
                    nullptr
                );
            }
        }

        // Initialise SwapChain & CommandContext
        m_swapChain.Initialise(factory, device, commandQueue, hwnd, m_width, m_height);
        m_commandContext.Initialise(device);

        // Initialise off-screen multisampled frame buffer
        m_frameBuffer.Initialise(
            device,
            m_width,
            m_height,
            m_swapChain.GetFormat(),
            DXGI_FORMAT_D32_FLOAT,
            4,
            m_clearColor
        );

        // Initialise combined shader pipelines (1 file per material containing VSMain and PSMain)
        // 1. Terrain shader (source/Shaders/Terrain.hlsl)
        Shader terrainVs;
        Shader terrainPs;
        const std::filesystem::path terrainPath = "source/Shaders/Terrain.hlsl";
        if (std::filesystem::exists(terrainPath))
        {
            terrainVs.CompileFromFile(terrainPath, "VSMain", ShaderStage::Vertex);
            terrainPs.CompileFromFile(terrainPath, "PSMain", ShaderStage::Pixel);
        }
        else
        {
            const std::string terrainSource = std::string(s_embeddedSceneBuffers) + s_embeddedVertexShaderStage + s_embeddedTerrainPixelStage;
            terrainVs.CompileFromSource(terrainSource, "EmbeddedTerrain.hlsl", "VSMain", ShaderStage::Vertex);
            terrainPs.CompileFromSource(terrainSource, "EmbeddedTerrain.hlsl", "PSMain", ShaderStage::Pixel);
        }

        // Initialise primary root signature, Terrain PipelineState, and Depth-Only Pre-Pass PipelineState
        m_pipelineState.Initialise(
            device,
            terrainVs,
            terrainPs,
            m_swapChain.GetFormat(),
            DXGI_FORMAT_D32_FLOAT,
            m_frameBuffer.GetSampleCount(),
            0,
            /* depthWrite = */ false,
            D3D12_COMPARISON_FUNC_GREATER_EQUAL
        );

        m_depthPipelineState.InitialiseDepthOnly(
            device,
            m_pipelineState.GetRootSignature(),
            terrainVs,
            DXGI_FORMAT_D32_FLOAT,
            m_frameBuffer.GetSampleCount(),
            0,
            D3D12_COMPARISON_FUNC_GREATER_EQUAL
        );

        // 2. Standard mesh shader (source/Shaders/Standard.hlsl)
        Shader standardVs;
        Shader standardPs;
        const std::filesystem::path standardPath = "source/Shaders/Standard.hlsl";
        if (std::filesystem::exists(standardPath))
        {
            standardVs.CompileFromFile(standardPath, "VSMain", ShaderStage::Vertex);
            standardPs.CompileFromFile(standardPath, "PSMain", ShaderStage::Pixel);
        }
        else
        {
            const std::string standardSource = std::string(s_embeddedSceneBuffers) + s_embeddedVertexShaderStage + s_embeddedStandardPixelStage;
            standardVs.CompileFromSource(standardSource, "EmbeddedStandard.hlsl", "VSMain", ShaderStage::Vertex);
            standardPs.CompileFromSource(standardSource, "EmbeddedStandard.hlsl", "PSMain", ShaderStage::Pixel);
        }

        PipelineState standardPso;
        standardPso.Initialise(
            device,
            m_pipelineState.GetRootSignature(),
            standardVs,
            standardPs,
            m_swapChain.GetFormat(),
            DXGI_FORMAT_D32_FLOAT,
            m_frameBuffer.GetSampleCount(),
            0,
            /* depthWrite = */ false,
            D3D12_COMPARISON_FUNC_GREATER_EQUAL
        );
        m_pipelineStates.emplace("Standard", std::move(standardPso));

        // 3. Unlit mesh shader (source/Shaders/Unlit.hlsl)
        Shader unlitVs;
        Shader unlitPs;
        const std::filesystem::path unlitPath = "source/Shaders/Unlit.hlsl";
        if (std::filesystem::exists(unlitPath))
        {
            unlitVs.CompileFromFile(unlitPath, "VSMain", ShaderStage::Vertex);
            unlitPs.CompileFromFile(unlitPath, "PSMain", ShaderStage::Pixel);
        }
        else
        {
            const std::string unlitSource = std::string(s_embeddedSceneBuffers) + s_embeddedVertexShaderStage + s_embeddedUnlitPixelStage;
            unlitVs.CompileFromSource(unlitSource, "EmbeddedUnlit.hlsl", "VSMain", ShaderStage::Vertex);
            unlitPs.CompileFromSource(unlitSource, "EmbeddedUnlit.hlsl", "PSMain", ShaderStage::Pixel);
        }

        m_unlitPipelineState.Initialise(
            device,
            m_pipelineState.GetRootSignature(),
            unlitVs,
            unlitPs,
            m_swapChain.GetFormat(),
            DXGI_FORMAT_D32_FLOAT,
            m_frameBuffer.GetSampleCount(),
            0,
            /* depthWrite = */ true,
            D3D12_COMPARISON_FUNC_GREATER_EQUAL
        );

        // Initialise Dynamic Constant Buffer Ring Allocator, Orientation Gizmo, and Diagnostic Text Overlay
        m_dynamicConstantBuffer.Initialise(device, DynamicUploadBuffer::DefaultPageSize, SwapChain::BufferCount);
        m_gizmoMesh = Mesh::CreateCoordinateAxes(device);

        m_textOverlay = std::make_unique<TextOverlay>();
        m_textOverlay->Initialise(device);
        // Initialise terrain PBR textures and GPU descriptor tables
        InitialiseTextureResources(device, commandQueue);

        m_lastFrameTime = std::chrono::high_resolution_clock::now();
        m_smoothedFps   = 120.0f;
        m_smoothedFrameTimeMs = 8.33f;
        m_fpsTimeAccumulator  = 0.0f;
        m_fpsFrameCount       = 0;

        m_camera->UpdateAspectRatio(static_cast<float>(m_width) / static_cast<float>(m_height));

        m_isInitialised = true;
        std::wcout << L"[Renderer] Renderer initialised successfully.\n";
    }

    void Renderer::Shutdown(ID3D12CommandQueue* commandQueue) noexcept
    {
        if (!m_isInitialised)
        {
            return;
        }

        if (m_textOverlay)
        {
            m_textOverlay->Shutdown();
            m_textOverlay.reset();
        }

        m_terrainTextures.clear();
        m_lidarOcclusionTexture.reset();
        m_srvHeap.Reset();
        m_commandContext.Shutdown(commandQueue);
        m_dynamicConstantBuffer.Shutdown();
        m_gizmoMesh.reset();
        m_frameBuffer.Shutdown();
        m_depthPipelineState = {};
        m_pipelineStates.clear();
        m_isInitialised = false;
    }

    PipelineState* Renderer::GetPipelineState(const std::string& shaderName) noexcept
    {
        if (shaderName == "Terrain")
        {
            return &m_pipelineState;
        }
        if (shaderName == "Unlit")
        {
            return &m_unlitPipelineState;
        }

        auto it = m_pipelineStates.find(shaderName);
        if (it != m_pipelineStates.end())
        {
            return &it->second;
        }
        auto standardIt = m_pipelineStates.find("Standard");
        if (standardIt != m_pipelineStates.end())
        {
            return &standardIt->second;
        }
        return &m_pipelineState;
    }

    const PipelineState* Renderer::GetPipelineState(const std::string& shaderName) const noexcept
    {
        if (shaderName == "Terrain")
        {
            return &m_pipelineState;
        }
        if (shaderName == "Unlit")
        {
            return &m_unlitPipelineState;
        }

        auto it = m_pipelineStates.find(shaderName);
        if (it != m_pipelineStates.end())
        {
            return &it->second;
        }
        auto standardIt = m_pipelineStates.find("Standard");
        if (standardIt != m_pipelineStates.end())
        {
            return &standardIt->second;
        }
        return &m_pipelineState;
    }

    void Renderer::OnResize(
        ID3D12Device* device,
        ID3D12CommandQueue* commandQueue,
        uint32_t width,
        uint32_t height
    )
    {
        if (!m_isInitialised || width == 0 || height == 0)
        {
            return;
        }

        m_width  = width;
        m_height = height;

        // Flush GPU before resizing swap chain back buffers and off-screen frame buffer
        m_commandContext.Flush(commandQueue);

        m_swapChain.Resize(device, m_width, m_height);
        m_frameBuffer.Resize(device, m_width, m_height);
        m_camera->UpdateAspectRatio(static_cast<float>(width) / static_cast<float>(height));
    }

    void Renderer::Render(
        ID3D12CommandQueue* commandQueue,
        std::span<const RenderItem> renderItems,
        std::span<const GpuLight> lights,
        bool vSync,
        size_t totalSceneItems,
        float updatesPerSecond,
        float targetUps
    )
    {
        const UINT frameIndex = m_swapChain.GetCurrentBackBufferIndex();
        m_commandContext.BeginFrame(frameIndex);
        m_dynamicConstantBuffer.BeginFrame(frameIndex);

        ID3D12GraphicsCommandList* const commandList = m_commandContext.GetCommandList();
        ID3D12Resource* const backBuffer = m_swapChain.GetCurrentRenderTarget();

        // Set root signature
        commandList->SetGraphicsRootSignature(m_pipelineState.GetRootSignature());

        // Bind texture descriptor heap and table across all passes
        if (m_srvHeap)
        {
            ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
            commandList->SetDescriptorHeaps(1, heaps);
            commandList->SetGraphicsRootDescriptorTable(2, m_srvHeap->GetGPUDescriptorHandleForHeapStart());
            commandList->SetGraphicsRoot32BitConstant(3, m_enableAlternativeTextures ? 1u : 0u, 0);
        }

        const Maths::Vec3D cameraPosition = m_camera ? m_camera->GetPosition() : Maths::Vec3D::Zero();

        // Build and sort the render queue into state-minimised batches
        m_renderQueue.Build(renderItems, cameraPosition, m_pipelineStates, m_pipelineState);

        // Pre-populate per-frame common scene lighting and atmospheric parameters
        SceneConstantBuffer commonCbData{};
        commonCbData.ambientColor = m_ambientColor;
        commonCbData.fogColor     = m_fogColor;
        commonCbData.fogParams    = m_fogParams;

        if (!lights.empty())
        {
            const uint32_t count = std::min(static_cast<uint32_t>(lights.size()), MaxLights);
            commonCbData.lightCount = count;
            for (uint32_t i = 0; i < count; ++i)
            {
                commonCbData.lights[i] = lights[i];
            }
        }
        else
        {
            commonCbData.lightCount = 1;
            commonCbData.lights[0]  = m_defaultLight;
        }

        // -------------------------------------------------------------
        // Pass 1: Depth Pre-Pass (Early-Z population, zero pixel shading)
        // -------------------------------------------------------------
        if (m_enableDepthPrePass && m_depthPipelineState.GetPipelineState())
        {
            // Bind depth-stencil buffer only (0 colour render targets) and clear depth
            m_frameBuffer.BindDepthOnly(commandList);
            m_frameBuffer.ClearDepth(commandList, 0.0f, 0);
            commandList->SetPipelineState(m_depthPipelineState.GetPipelineState());

            const Mesh* depthBoundMesh = nullptr;
            for (const auto& batch : m_renderQueue.GetOpaqueBatches())
            {
                if (batch.items.empty() || !batch.mesh)
                {
                    continue;
                }

                if (batch.mesh != depthBoundMesh)
                {
                    depthBoundMesh = batch.mesh;
                    depthBoundMesh->Bind(commandList);
                }

                const size_t instanceCount = batch.items.size();
                const bool canUseDirectPath = (instanceCount == 1 && batch.items[0]->colorTint == Maths::Vec4::One());

                if (canUseDirectPath)
                {
                    const auto* item = batch.items[0];
                    SceneConstantBuffer cbData = commonCbData;
                    cbData.mvp         = m_camera->CalculateCameraRelativeMVP(item->worldMatrix);
                    cbData.world       = m_camera ? m_camera->CalculateCameraRelativeWorld(item->worldMatrix) : Maths::Mat4x4(item->worldMatrix);
                    cbData.isInstanced = 0;

                    const DynamicAllocation cbAlloc = m_dynamicConstantBuffer.Allocate(cbData);
                    commandList->SetGraphicsRootConstantBufferView(0, cbAlloc.gpuAddress);

                    depthBoundMesh->DrawBound(commandList, 1, 0);
                }
                else
                {
                    SceneConstantBuffer cbData = commonCbData;
                    cbData.mvp         = Maths::Mat4x4::Identity();
                    cbData.world       = Maths::Mat4x4::Identity();
                    cbData.isInstanced = 1;

                    const DynamicAllocation cbAlloc = m_dynamicConstantBuffer.Allocate(cbData);
                    commandList->SetGraphicsRootConstantBufferView(0, cbAlloc.gpuAddress);

                    const size_t byteSize = instanceCount * sizeof(GpuInstanceData);
                    const DynamicAllocation instAlloc = m_dynamicConstantBuffer.Allocate(byteSize, 16);
                    auto* dest = static_cast<GpuInstanceData*>(instAlloc.cpuAddress);

                    for (size_t i = 0; i < instanceCount; ++i)
                    {
                        const auto* item = batch.items[i];
                        dest[i].cameraRelativeMVP   = m_camera->CalculateCameraRelativeMVP(item->worldMatrix);
                        dest[i].cameraRelativeWorld = m_camera ? m_camera->CalculateCameraRelativeWorld(item->worldMatrix) : Maths::Mat4x4(item->worldMatrix);
                        dest[i].colorTint           = item->colorTint;
                    }

                    commandList->SetGraphicsRootShaderResourceView(1, instAlloc.gpuAddress);
                    depthBoundMesh->DrawBound(commandList, static_cast<uint32_t>(instanceCount), 0);
                }
            }
        }

        // -------------------------------------------------------------
        // Pass 2: Main Scene Forward Shading Pass (with Early-Z depth test)
        // -------------------------------------------------------------
        m_frameBuffer.Bind(commandList);
        m_frameBuffer.ClearRenderTarget(commandList);
        if (!m_enableDepthPrePass || !m_depthPipelineState.GetPipelineState())
        {
            m_frameBuffer.ClearDepth(commandList, 0.0f, 0);
        }

        ID3D12PipelineState* currentPso = nullptr;
        const Mesh* currentBoundMesh    = nullptr;

        auto executeBatches = [&](std::span<const RenderBatch> batches)
        {
            for (const auto& batch : batches)
            {
                if (batch.items.empty() || !batch.mesh)
                {
                    continue;
                }

                // Pipeline state transition: update only when pipeline state changes
                ID3D12PipelineState* const targetPso = (batch.pso && batch.pso->GetPipelineState())
                    ? batch.pso->GetPipelineState()
                    : m_pipelineState.GetPipelineState();

                if (currentPso != targetPso && targetPso)
                {
                    currentPso = targetPso;
                    commandList->SetPipelineState(currentPso);
                }

                // Input Assembler binding: bind buffers only when mesh geometry changes
                if (batch.mesh != currentBoundMesh)
                {
                    currentBoundMesh = batch.mesh;
                    currentBoundMesh->Bind(commandList);
                }

                const size_t instanceCount = batch.items.size();
                const bool canUseDirectPath = (instanceCount == 1 && batch.items[0]->colorTint == Maths::Vec4::One());

                if (canUseDirectPath)
                {
                    // Single untinted instance direct path (e.g. landscape terrain mesh)
                    const auto* item = batch.items[0];
                    SceneConstantBuffer cbData = commonCbData;
                    cbData.mvp                 = m_camera->CalculateCameraRelativeMVP(item->worldMatrix);
                    cbData.world               = m_camera ? m_camera->CalculateCameraRelativeWorld(item->worldMatrix) : Maths::Mat4x4(item->worldMatrix);
                    cbData.isInstanced         = 0;
                    cbData.objectLightChannels = item->lightChannels;

                    const DynamicAllocation cbAlloc = m_dynamicConstantBuffer.Allocate(cbData);
                    commandList->SetGraphicsRootConstantBufferView(0, cbAlloc.gpuAddress);

                    currentBoundMesh->DrawBound(commandList, 1, 0);
                }
                else
                {
                    // Hardware instanced multi-item dispatch: 1 draw call across all N instances
                    SceneConstantBuffer cbData = commonCbData;
                    cbData.mvp                 = Maths::Mat4x4::Identity();
                    cbData.world               = Maths::Mat4x4::Identity();
                    cbData.isInstanced         = 1;
                    cbData.objectLightChannels = batch.items[0]->lightChannels;

                    const DynamicAllocation cbAlloc = m_dynamicConstantBuffer.Allocate(cbData);
                    commandList->SetGraphicsRootConstantBufferView(0, cbAlloc.gpuAddress);

                    const size_t byteSize = instanceCount * sizeof(GpuInstanceData);
                    const DynamicAllocation instAlloc = m_dynamicConstantBuffer.Allocate(byteSize, 16);
                    auto* dest = static_cast<GpuInstanceData*>(instAlloc.cpuAddress);

                    for (size_t i = 0; i < instanceCount; ++i)
                    {
                        const auto* item = batch.items[i];
                        dest[i].cameraRelativeMVP   = m_camera->CalculateCameraRelativeMVP(item->worldMatrix);
                        dest[i].cameraRelativeWorld = m_camera ? m_camera->CalculateCameraRelativeWorld(item->worldMatrix) : Maths::Mat4x4(item->worldMatrix);
                        dest[i].colorTint           = item->colorTint;
                    }

                    commandList->SetGraphicsRootShaderResourceView(1, instAlloc.gpuAddress);
                    currentBoundMesh->DrawBound(commandList, static_cast<uint32_t>(instanceCount), 0);
                }
            }
        };

        // Execute opaque batches, followed by transparent batches
        executeBatches(m_renderQueue.GetOpaqueBatches());
        executeBatches(m_renderQueue.GetTransparentBatches());

        if (currentPso != m_pipelineState.GetPipelineState())
        {
            commandList->SetPipelineState(m_pipelineState.GetPipelineState());
        }

        // Render World-Space Orientation Gizmo in the top-left corner
        if (m_showGizmo && m_gizmoMesh && m_gizmoMesh->IsInitialised())
        {
            const float marginX = m_gizmoMarginX;
            const float marginY = m_gizmoMarginY;
            const float size    = m_gizmoSize;

            D3D12_VIEWPORT gizmoViewport{};
            gizmoViewport.TopLeftX = marginX;
            gizmoViewport.TopLeftY = marginY;
            gizmoViewport.Width    = size;
            gizmoViewport.Height   = size;
            gizmoViewport.MinDepth = 0.0f;
            gizmoViewport.MaxDepth = 1.0f;

            D3D12_RECT gizmoScissor{};
            gizmoScissor.left   = static_cast<LONG>(marginX);
            gizmoScissor.top    = static_cast<LONG>(marginY);
            gizmoScissor.right  = static_cast<LONG>(marginX + size);
            gizmoScissor.bottom = static_cast<LONG>(marginY + size);

            commandList->RSSetViewports(1, &gizmoViewport);
            commandList->RSSetScissorRects(1, &gizmoScissor);

            // Clear depth within gizmo region so it renders on top of scene geometry while depth-testing against itself
            m_frameBuffer.ClearDepthScissor(commandList, gizmoScissor, 0.0f);

            // Extract camera's view rotation matrix and offset along view Z
            const auto rot = m_camera->GetViewMatrix().GetRotationMatrix();
            Maths::Mat4x4 gizmoView(
                static_cast<float>(rot.m[0][0]), static_cast<float>(rot.m[0][1]), static_cast<float>(rot.m[0][2]), 0.0f,
                static_cast<float>(rot.m[1][0]), static_cast<float>(rot.m[1][1]), static_cast<float>(rot.m[1][2]), 0.0f,
                static_cast<float>(rot.m[2][0]), static_cast<float>(rot.m[2][1]), static_cast<float>(rot.m[2][2]), 0.0f,
                0.0f,                            0.0f,                            3.0f,                            1.0f
            );

            // Square orthographic projection to prevent perspective distortion at corner
            const Maths::Mat4x4 gizmoProj = Maths::Mat4x4::Orthographic(2.8f, 2.8f, 0.1f, 10.0f);

            SceneConstantBuffer gizmoCb;
            gizmoCb.mvp          = gizmoView * gizmoProj;
            gizmoCb.world        = Maths::Mat4x4::Identity();
            // Unlit shading: zero diffuse contribution and unit ambient multiplier
            gizmoCb.ambientColor = Maths::Vec4::One();
            gizmoCb.fogColor     = Maths::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
            gizmoCb.fogParams    = Maths::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
            gizmoCb.lightCount   = 0;
            gizmoCb.isInstanced  = 0;

            const PipelineState* unlitPso = GetPipelineState("Unlit");
            if (unlitPso && unlitPso->GetPipelineState())
            {
                commandList->SetPipelineState(unlitPso->GetPipelineState());
            }

            const DynamicAllocation gizmoAlloc = m_dynamicConstantBuffer.Allocate(gizmoCb);
            commandList->SetGraphicsRootConstantBufferView(0, gizmoAlloc.gpuAddress);
            m_gizmoMesh->Draw(commandList);

            // Restore primary viewport and scissor rect from frame buffer
            commandList->RSSetViewports(1, &m_frameBuffer.GetViewport());
            commandList->RSSetScissorRects(1, &m_frameBuffer.GetScissorRect());
        }

        // Compute instantaneous and periodically averaged frame rate, targetting m_targetFps and no more
        const auto currentTime = std::chrono::high_resolution_clock::now();
        const float dt = std::chrono::duration<float>(currentTime - m_lastFrameTime).count();
        m_lastFrameTime = currentTime;

        const float maxTargetFps = m_targetFps;
        const float minFrameTimeMs = (maxTargetFps > 0.0f) ? (1000.0f / maxTargetFps) : 8.33f;

        if (dt > 0.0f && dt < 1.0f)
        {
            m_fpsTimeAccumulator += dt;
            m_fpsFrameCount++;

            // Periodically update diagnostic FPS metrics (every 250 ms) to eliminate sub-millisecond OS scheduling jitter and stabilise display
            if (m_fpsTimeAccumulator >= 0.25f)
            {
                const float measuredFps = static_cast<float>(m_fpsFrameCount) / m_fpsTimeAccumulator;
                const float measuredFrameTimeMs = (m_fpsTimeAccumulator / static_cast<float>(m_fpsFrameCount)) * 1000.0f;

                m_smoothedFps = std::min(measuredFps, maxTargetFps);
                m_smoothedFrameTimeMs = std::max(measuredFrameTimeMs, minFrameTimeMs);

                m_fpsTimeAccumulator = 0.0f;
                m_fpsFrameCount = 0;
            }
        }
        else if (dt >= 1.0f)
        {
            m_fpsTimeAccumulator = 0.0f;
            m_fpsFrameCount = 0;
        }

        // Render Diagnostic Text Overlay in the top-right corner (opposite the orientation gizmo)
        if (m_showOverlay && m_textOverlay && m_textOverlay->IsInitialised())
        {

            // Sum up total triangles and vertices across visible render items
            size_t visibleMeshCount = 0;
            size_t totalTriangles   = 0;
            size_t totalVertices    = 0;
            for (const auto& item : renderItems)
            {
                if (item.isVisible && item.mesh)
                {
                    ++visibleMeshCount;
                    totalTriangles += item.mesh->GetTriangleCount();
                    totalVertices  += item.mesh->GetVertexCount();
                }
            }

            // Total scene items include renderable meshes, the camera entity, and active light entities
            const size_t cameraCount = (m_camera != nullptr) ? 1 : 0;
            const size_t lightCount  = lights.size();
            const size_t sceneItemCount = (totalSceneItems > 0) ? totalSceneItems : (visibleMeshCount + cameraCount + lightCount);

            OverlayStatistics stats{};
            stats.gpuName       = m_gpuName;
            stats.fps           = std::min(m_smoothedFps, maxTargetFps);
            stats.ups           = updatesPerSecond;
            stats.targetFps     = m_targetFps;
            stats.targetUps     = (targetUps > 0.0f) ? targetUps : m_targetUps;
            stats.frameTimeMs   = std::max(m_smoothedFrameTimeMs, minFrameTimeMs);
            stats.sampleCount   = m_frameBuffer.GetSampleCount();
            stats.objectCount   = sceneItemCount;
            stats.itemCount     = sceneItemCount;
            stats.triangleCount = totalTriangles;
            stats.vertexCount   = totalVertices;
            stats.lightCount    = static_cast<uint32_t>(lightCount);

            const PipelineState* unlitPso = GetPipelineState("Unlit");
            if (unlitPso && unlitPso->GetPipelineState())
            {
                commandList->SetPipelineState(unlitPso->GetPipelineState());
            }

            m_textOverlay->Update(frameIndex, stats, m_width, m_height);
            m_textOverlay->Render(commandList, frameIndex, m_width, m_height, m_frameBuffer.GetDsvHandle());

            // Restore primary viewport and scissor rect from frame buffer
            commandList->RSSetViewports(1, &m_frameBuffer.GetViewport());
            commandList->RSSetScissorRects(1, &m_frameBuffer.GetScissorRect());
        }

        // Hardware MSAA resolve or blit off-screen frame buffer into destination swap chain back buffer
        m_frameBuffer.Resolve(commandList, backBuffer, D3D12_RESOURCE_STATE_PRESENT);

        // Execute command list on GPU and present frame
        m_commandContext.Execute(commandQueue, frameIndex);
        m_swapChain.Present(vSync);
    }

    void Renderer::InitialiseTextureResources(ID3D12Device* device, ID3D12CommandQueue* commandQueue)
    {
        if (!device || !commandQueue)
        {
            return;
        }

        // Allocate shader-visible SRV descriptor heap for scene textures
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.NumDescriptors = 48;
        heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        heapDesc.NodeMask       = 0;

        HR_CHECK(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_srvHeap)));
        m_srvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // List of terrain texture maps corresponding to shader slots t1 through t40
        const std::filesystem::path texDir = "resources/environment/terrain/textures";
        const std::vector<std::string> textureFiles = {
            // Low lying pasture and meadow
            "uncut_grass_oilpt20_4k_albedo.dds",
            "uncut_grass_oilpt20_4k_normal.dds",
            "uncut_grass_oilpt20_4k_roughness.dds",
            "uncut_grass_oilpt20_4k_ao.dds",

            // Mid slope fell turf
            "wild_grass_umjlabus_4k_albedo.dds",
            "wild_grass_umjlabus_4k_normal.dds",
            "wild_grass_umjlabus_4k_roughness.dds",
            "wild_grass_umjlabus_4k_ao.dds",

            // High plateau moorland
            "wild_grass_vbslfeqfw_4k_albedo.dds",
            "wild_grass_vbslfeqfw_4k_normal.dds",
            "wild_grass_vbslfeqfw_4k_roughness.dds",
            "wild_grass_vbslfeqfw_4k_ao.dds",

            // Dry thatch knolls and ridges
            "grass_dried_olqkj0_4k_albedo.dds",
            "grass_dried_olqkj0_4k_normal.dds",
            "grass_dried_olqkj0_4k_roughness.dds",
            "grass_dried_olqkj0_4k_ao.dds",

            // Crevice and sheer rock cliff
            "rock_cliff_vl3ibcxlw_4k_albedo.dds",
            "rock_cliff_vl3ibcxlw_4k_normal.dds",
            "rock_cliff_vl3ibcxlw_4k_roughness.dds",
            "rock_cliff_vl3ibcxlw_4k_ao.dds",

            // Displacement heightmaps for Parallax Occlusion Mapping
            "uncut_grass_oilpt20_4k_displacement.dds",
            "wild_grass_umjlabus_4k_displacement.dds",
            "wild_grass_vbslfeqfw_4k_displacement.dds",
            "grass_dried_olqkj0_4k_displacement.dds",
            "rock_cliff_vl3ibcxlw_4k_displacement.dds",

            // Alternative low-lying meadow (clover and grazed pasture)
            "uncut_grass_pftph0_4k_albedo.dds",
            "uncut_grass_pftph0_4k_normal.dds",
            "uncut_grass_pftph0_4k_roughness.dds",
            "uncut_grass_pftph0_4k_ao.dds",

            // Alternative mid-slope (fine moor-edge bent-grass)
            "wild_grass_umrmdgps_4k_albedo.dds",
            "wild_grass_umrmdgps_4k_normal.dds",
            "wild_grass_umrmdgps_4k_roughness.dds",
            "wild_grass_umrmdgps_4k_ao.dds",

            // Alternative high fell (weathered crag turf)
            "wild_grass_vbikagyn_4k_albedo.dds",
            "wild_grass_vbikagyn_4k_normal.dds",
            "wild_grass_vbikagyn_4k_roughness.dds",
            "wild_grass_vbikagyn_4k_ao.dds",

            // Alternative displacement heightmaps
            "uncut_grass_pftph0_4k_displacement.dds",
            "wild_grass_umrmdgps_4k_displacement.dds",
            "wild_grass_vbikagyn_4k_displacement.dds"
        };

        ComPtr<ID3D12CommandAllocator> uploadAlloc;
        HR_CHECK(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&uploadAlloc)));

        ComPtr<ID3D12GraphicsCommandList> uploadCmdList;
        HR_CHECK(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, uploadAlloc.Get(), nullptr, IID_PPV_ARGS(&uploadCmdList)));

        std::vector<ComPtr<ID3D12Resource>> stagingBuffers;
        stagingBuffers.reserve(textureFiles.size());
        m_terrainTextures.reserve(textureFiles.size());

        D3D12_CPU_DESCRIPTOR_HANDLE heapStart = m_srvHeap->GetCPUDescriptorHandleForHeapStart();

        for (size_t i = 0; i < textureFiles.size(); ++i)
        {
            const auto filePath = texDir / textureFiles[i];
            if (std::filesystem::exists(filePath))
            {
                try
                {
                    ComPtr<ID3D12Resource> staging;
                    auto texture = Texture::LoadFromDds(device, uploadCmdList.Get(), filePath, staging, textureFiles[i]);
                    if (texture)
                    {
                        D3D12_CPU_DESCRIPTOR_HANDLE destHandle = heapStart;
                        destHandle.ptr += i * m_srvDescriptorSize;
                        texture->CreateShaderResourceView(device, destHandle);

                        m_terrainTextures.push_back(texture);
                        stagingBuffers.push_back(staging);
                    }
                }
                catch (const std::exception& e)
                {
                    std::wcout << L"[Renderer] Warning: Could not load texture " << filePath.wstring().c_str()
                               << L": " << e.what() << L"\n";
                }
            }
        }

        // Initialise fallback texture at descriptor slot 40 (register t41)
        constexpr uint32_t defaultLidarPixel = 0xFF0000FF; // R=255 (AO=1.0), G=0 (crevice=0.0), B=0, A=255
        ComPtr<ID3D12Resource> fallbackStaging;
        auto fallbackLidarTexture = Texture::Create2D(
            device,
            uploadCmdList.Get(),
            &defaultLidarPixel,
            1,
            1,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            fallbackStaging,
            "DefaultLidarOcclusionFallback"
        );
        stagingBuffers.push_back(fallbackStaging);
        constexpr uint32_t lidarSrvSlot = 40;
        D3D12_CPU_DESCRIPTOR_HANDLE fallbackHandle = heapStart;
        fallbackHandle.ptr += lidarSrvSlot * m_srvDescriptorSize;
        fallbackLidarTexture->CreateShaderResourceView(device, fallbackHandle);
        m_lidarOcclusionTexture = fallbackLidarTexture;

        HR_CHECK(uploadCmdList->Close());
        ID3D12CommandList* lists[] = { uploadCmdList.Get() };
        commandQueue->ExecuteCommandLists(1, lists);

        // Synchronise with GPU queue to ensure staging buffers can be safely released
        ComPtr<ID3D12Fence> fence;
        HR_CHECK(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
        HR_CHECK(commandQueue->Signal(fence.Get(), 1));
        HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (event)
        {
            HR_CHECK(fence->SetEventOnCompletion(1, event));
            WaitForSingleObject(event, INFINITE);
            CloseHandle(event);
        }

        std::wcout << L"[Renderer] Loaded " << m_terrainTextures.size()
                   << L" terrain PBR textures into GPU descriptor table.\n";
    }

    void Renderer::SetLidarOcclusionMap(
        ID3D12Device* device,
        ID3D12CommandQueue* commandQueue,
        const void* pixelData,
        uint32_t width,
        uint32_t height
    )
    {
        if (!device || !commandQueue || !pixelData || width == 0 || height == 0 || !m_srvHeap)
        {
            return;
        }

        ComPtr<ID3D12CommandAllocator> uploadAlloc;
        HR_CHECK(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&uploadAlloc)));

        ComPtr<ID3D12GraphicsCommandList> uploadCmdList;
        HR_CHECK(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, uploadAlloc.Get(), nullptr, IID_PPV_ARGS(&uploadCmdList)));

        ComPtr<ID3D12Resource> stagingBuffer;
        m_lidarOcclusionTexture = Texture::Create2D(
            device,
            uploadCmdList.Get(),
            pixelData,
            width,
            height,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            stagingBuffer,
            "LidarHorizonOcclusion"
        );

        // Bind to descriptor slot 40 (register t41) in m_srvHeap
        constexpr uint32_t lidarSrvSlot = 40;
        D3D12_CPU_DESCRIPTOR_HANDLE destHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
        destHandle.ptr += lidarSrvSlot * m_srvDescriptorSize;
        m_lidarOcclusionTexture->CreateShaderResourceView(device, destHandle);

        HR_CHECK(uploadCmdList->Close());
        ID3D12CommandList* lists[] = { uploadCmdList.Get() };
        commandQueue->ExecuteCommandLists(1, lists);

        // Synchronise with GPU queue to ensure staging buffer can be safely released
        ComPtr<ID3D12Fence> fence;
        HR_CHECK(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
        HR_CHECK(commandQueue->Signal(fence.Get(), 1));
        HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (event)
        {
            HR_CHECK(fence->SetEventOnCompletion(1, event));
            WaitForSingleObject(event, INFINITE);
            CloseHandle(event);
        }

        std::wcout << L"[Renderer] Bound pre-baked LiDAR horizon occlusion and crevice depth map ("
                   << width << L"x" << height << L") to slot t41.\n";
    }
}

