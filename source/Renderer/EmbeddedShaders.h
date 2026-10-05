// Copyright © 2026 spacegirl65. All Rights Reserved.

#pragma once

namespace Sandbox3D::Renderer
{
    // Embedded fallback HLSL source ensuring runtime rendering capability when external shader files are unavailable
    inline constexpr const char* s_embeddedSceneBuffers = R"(
        struct LightData
        {
            float4 position;    // Position in xyz, and range in w
            float4 direction;   // Normalized direction vector in xyz, and light type in w
            float4 color;       // Light colour in rgb, and intensity in w
            float4 attenuation; // Attenuation factors (constant, linear, quadratic) in xyz, and spot cosine in w
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
            row_major float4x4 g_shadowViewProj[4];
            float4             g_cascadeSplits;
            float4             g_shadowParams;
            float4             g_rayleighParams;
            float4             g_mieParams;
            float4             g_ozoneParams;
            float4             g_atmosphereParams;
        };

        StructuredBuffer<InstanceData> g_instances : register(t0);

        Texture2DArray g_shadowMapArray : register(t44);
        SamplerComparisonState g_samplerShadow : register(s2);

        float EvaluateCascadeSliceShadow(float3 worldPosition, float3 worldNormal, float3 lightDir, int cascadeIndex)
        {
            float4 lightClip = mul(float4(worldPosition, 1.0f), g_shadowViewProj[cascadeIndex]);
            float3 shadowCoord = float3(lightClip.x * 0.5f + 0.5f, -lightClip.y * 0.5f + 0.5f, lightClip.z);
            if (shadowCoord.z < 0.0f || shadowCoord.z > 1.0f) return 1.0f;
            if (shadowCoord.x < 0.001f || shadowCoord.x > 0.999f || shadowCoord.y < 0.001f || shadowCoord.y > 0.999f) return 1.0f;
            float nDotL = max(dot(worldNormal, lightDir), 0.0f);
            float slopeFactor = min(sqrt(saturate(1.0f - nDotL * nDotL)) / max(nDotL, 0.001f), 4.0f);
            float cascadeScale = float(cascadeIndex + 1);
            float bias = (0.00005f + 0.00020f * slopeFactor) * cascadeScale;
            float shadow = 0.0f;
            const float texelSize = 1.0f / 2048.0f;
            const float2 offsets[4] = {
                float2(-0.7071f, -0.7071f) * texelSize,
                float2( 0.7071f, -0.7071f) * texelSize,
                float2(-0.7071f,  0.7071f) * texelSize,
                float2( 0.7071f,  0.7071f) * texelSize
            };
            [unroll]
            for (int tap = 0; tap < 4; ++tap)
            {
                shadow += g_shadowMapArray.SampleCmpLevelZero(g_samplerShadow, float3(shadowCoord.xy + offsets[tap], float(cascadeIndex)), shadowCoord.z - bias);
            }
            return shadow * 0.25f;
        }

        float CalculateCascadedShadow(float3 worldPosition, float3 worldNormal, float3 lightDir)
        {
            const float viewDepth = length(worldPosition);
            int cascadeIndex = 3;
            if (viewDepth < g_cascadeSplits.x) cascadeIndex = 0;
            else if (viewDepth < g_cascadeSplits.y) cascadeIndex = 1;
            else if (viewDepth < g_cascadeSplits.z) cascadeIndex = 2;
            else if (viewDepth >= g_cascadeSplits.w) return 1.0f;
            float shadow = EvaluateCascadeSliceShadow(worldPosition, worldNormal, lightDir, cascadeIndex);
            if (cascadeIndex < 3)
            {
                float splitDist = g_cascadeSplits[cascadeIndex];
                float blendStart = splitDist * 0.92f;
                if (viewDepth > blendStart)
                {
                    float blendFactor = saturate((viewDepth - blendStart) / (splitDist - blendStart));
                    float nextShadow = EvaluateCascadeSliceShadow(worldPosition, worldNormal, lightDir, cascadeIndex + 1);
                    shadow = lerp(shadow, nextShadow, blendFactor);
                }
            }
            return shadow;
        }
    )";

    inline constexpr const char* s_embeddedVertexShaderStage = R"(
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

    inline constexpr const char* s_embeddedTerrainPixelStage = R"(
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
                    if (nDotL > 0.0f)
                    {
                        const float csmShadow = (i == 0u) ? CalculateCascadedShadow(input.worldPosition, N, L) : 1.0f;
                        totalDiffuse += lightRgb * (nDotL * csmShadow);

                        float3 H = normalize(L + V);
                        float nDotH = max(dot(N, H), 0.0f);
                        totalSpecular += lightRgb * (pow(nDotH, specPower) * specIntensity * csmShadow);
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
                        falloff = saturate(1.0f - (dist / range));
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

    inline constexpr const char* s_embeddedStandardPixelStage = R"(
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
                    float nDotL = max(dot(N, L), 0.0f);
                    if (nDotL > 0.0f)
                    {
                        const float csmShadow = (i == 0u) ? CalculateCascadedShadow(input.worldPosition, N, L) : 1.0f;
                        radiance = lightRgb * csmShadow;
                    }
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

    inline constexpr const char* s_embeddedUnlitPixelStage = R"(
        float4 PSMain(VertexOutput input) : SV_TARGET
        {
            return input.color;
        }
    )";
}

