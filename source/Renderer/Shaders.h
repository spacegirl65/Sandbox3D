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
            float4             materialParams;
        };

        #define morphParams materialParams

        cbuffer SceneConstantBuffer : register(b0)
        {
            row_major float4x4 g_mvp;
            row_major float4x4 g_world;
            float4             g_ambientColor;
            float4             g_fogColor;
            float4             g_fogParams;
            float4             g_cameraPosition;
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
            float4             g_exposureParams;
            float4             g_adaptationParams;
        };

        StructuredBuffer<InstanceData> g_instances : register(t0);

        Texture2DArray g_shadowMapArray : register(t28);
        SamplerComparisonState g_samplerShadow : register(s2);

        float EvaluateCascadeSliceShadow(float3 worldPosition, float3 worldNormal, float3 lightDir, int cascadeIndex)
        {
            const float nDotL = max(dot(worldNormal, lightDir), 0.0f);
            const float sinTheta = sqrt(saturate(1.0f - nDotL * nDotL));
            const float slopeFactor = min(sinTheta / max(nDotL, 0.001f), 4.0f);
            const float normalOffsetMeters[4] = { 0.04f, 0.16f, 0.60f, 2.20f };
            const float3 biasedWorldPosition = worldPosition + worldNormal * (normalOffsetMeters[cascadeIndex] * sinTheta);
            float4 lightClip = mul(float4(biasedWorldPosition, 1.0f), g_shadowViewProj[cascadeIndex]);
            float3 shadowCoord = float3(lightClip.x * 0.5f + 0.5f, -lightClip.y * 0.5f + 0.5f, lightClip.z);
            if (shadowCoord.z < 0.0f || shadowCoord.z > 1.0f) return 1.0f;
            if (shadowCoord.x < 0.001f || shadowCoord.x > 0.999f || shadowCoord.y < 0.001f || shadowCoord.y > 0.999f) return 1.0f;
            const float cascadeScale = float(cascadeIndex + 1);
            const float bias = (0.00006f + 0.00022f * slopeFactor) * cascadeScale;
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

        float3 AcesFilmicToneMapping(float3 x)
        {
            const float a = 2.51f;
            const float b = 0.03f;
            const float c = 2.43f;
            const float d = 0.59f;
            const float e = 0.14f;
            return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
        }

        float3 LinearToSrgb(float3 linearColor)
        {
            float3 srgb;
            srgb.r = (linearColor.r <= 0.0031308f) ? (linearColor.r * 12.92f) : (1.055f * pow(linearColor.r, 1.0f / 2.4f) - 0.055f);
            srgb.g = (linearColor.g <= 0.0031308f) ? (linearColor.g * 12.92f) : (1.055f * pow(linearColor.g, 1.0f / 2.4f) - 0.055f);
            srgb.b = (linearColor.b <= 0.0031308f) ? (linearColor.b * 12.92f) : (1.055f * pow(linearColor.b, 1.0f / 2.4f) - 0.055f);
            return srgb;
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
            float4 position       : SV_POSITION;
            float3 worldNormal    : NORMAL;
            float4 color          : COLOR;
            float3 worldPosition  : TEXCOORD0;
            float4 materialParams : TEXCOORD1;
        };

        VertexOutput VSMain(VertexInput input, uint instanceId : SV_InstanceID)
        {
            VertexOutput output;

            if (g_isInstanced != 0)
            {
                InstanceData inst     = g_instances[instanceId];
                output.position       = mul(float4(input.position, 1.0f), inst.mvp);
                output.worldNormal    = normalize(mul(float4(input.normal, 0.0f), inst.world).xyz);
                output.worldPosition  = mul(float4(input.position, 1.0f), inst.world).xyz;
                output.color          = input.color * inst.colorTint;
                output.materialParams = inst.materialParams;
            }
            else
            {
                output.position       = mul(float4(input.position, 1.0f), g_mvp);
                output.worldNormal    = normalize(mul(float4(input.normal, 0.0f), g_world).xyz);
                output.worldPosition  = mul(float4(input.position, 1.0f), g_world).xyz;
                output.color          = input.color;
                output.materialParams = float4(0.5f, 0.0f, 0.5f, 1.0f);
            }

            return output;
        }
    )";

    inline constexpr const char* s_embeddedTerrainPixelStage = R"(
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

            return float4(max(shadedColor, 0.0f), 1.0f);
        }
    )";

    inline constexpr const char* s_embeddedStandardPixelStage = R"(
        TextureCube g_celestialCube : register(t32);
        SamplerState g_celestialSampler : register(s1);

        static const float PI = 3.14159265359f;

        // Trowbridge-Reitz GGX normal distribution function (NDF) with Disney perceptual roughness
        float DistributionGGX(float3 N, float3 H, float roughness)
        {
            const float alpha  = max(roughness * roughness, 0.001f);
            const float alpha2 = alpha * alpha;
            const float nDotH  = max(dot(N, H), 0.0f);
            const float nDotH2 = nDotH * nDotH;

            const float denom = (nDotH2 * (alpha2 - 1.0f) + 1.0f);
            return alpha2 / max(PI * denom * denom, 0.0000001f);
        }

        // Height-correlated Smith geometric visibility function (Heitz 2014)
        float VisibilitySmithGGXCorrelated(float nDotV, float nDotL, float roughness)
        {
            const float alpha   = max(roughness * roughness, 0.001f);
            const float alpha2  = alpha * alpha;
            const float lambdaV = nDotL * sqrt(nDotV * nDotV * (1.0f - alpha2) + alpha2);
            const float lambdaL = nDotV * sqrt(nDotL * nDotL * (1.0f - alpha2) + alpha2);
            return 0.5f / max(lambdaV + lambdaL, 0.00001f);
        }

        // Fresnel-Schlick approximation evaluating surface reflectance at glancing angles
        float3 FresnelSchlick(float cosTheta, float3 F0)
        {
            return F0 + (1.0f - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
        }

        // Fresnel-Schlick approximation incorporating roughness attenuation at grazing angles
        float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
        {
            const float3 maxF = max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), F0);
            return F0 + (maxF - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
        }

        // Karis split-sum environment BRDF polynomial approximation (F0 * scale + bias)
        float2 EnvBRDFApprox(float roughness, float nDotX)
        {
            const float4 c0 = float4(-1.0f, -0.0275f, -0.572f, 0.022f);
            const float4 c1 = float4( 1.0f,  0.0425f,  1.040f, -0.040f);
            float4 rParam = roughness * c0 + c1;
            float a004 = min(rParam.x * rParam.x, exp2(-9.28f * nDotX)) * rParam.x + rParam.y;
            return float2(-1.04f, 1.04f) * a004 + rParam.zw;
        }

        // Trowbridge-Reitz GGX anisotropic normal distribution function (Burley 2012 / Heitz 2014)
        float DistributionGGXAnisotropic(float3 H, float3 T, float3 B, float3 N, float at, float ab)
        {
            const float hDotT = dot(H, T);
            const float hDotB = dot(H, B);
            const float hDotN = max(dot(H, N), 0.0f);

            const float d = (hDotT * hDotT) / (at * at) + (hDotB * hDotB) / (ab * ab) + (hDotN * hDotN);
            return 1.0f / max(PI * at * ab * d * d, 0.0000001f);
        }

        // Height-correlated Smith geometric visibility function for anisotropic microfacets (Heitz 2014)
        float VisibilitySmithGGXCorrelatedAnisotropic(float3 V, float3 L, float3 T, float3 B, float3 N, float at, float ab)
        {
            const float vDotT = dot(V, T);
            const float vDotB = dot(V, B);
            const float vDotN = max(dot(V, N), 0.0001f);

            const float lDotT = dot(L, T);
            const float lDotB = dot(L, B);
            const float lDotN = max(dot(L, N), 0.0001f);

            const float lambdaV = lDotN * sqrt(vDotN * vDotN + (vDotT * at) * (vDotT * at) + (vDotB * ab) * (vDotB * ab));
            const float lambdaL = vDotN * sqrt(lDotN * lDotN + (lDotT * at) * (lDotT * at) + (lDotB * ab) * (lDotB * ab));
            return 0.5f / max(lambdaV + lambdaL, 0.00001f);
        }

        // Brian Karis representative point on the solar disk (angular diameter = 0.53 degrees, angular radius = 0.004625 rad)
        float3 GetSolarDiskLightDirection(float3 L_dir, float3 V, float3 N, float sunAngularRadius)
        {
            float3 R = reflect(-V, N);
            float3 D = R - dot(R, L_dir) * L_dir;
            float lenD = length(D);
            float3 L_rep = L_dir + D * (lenD > 0.00001f ? min(1.0f, sunAngularRadius / lenD) : 0.0f);
            return normalize(L_rep);
        }

        // Fast integer bit-mixing hash for procedural micro-grain (PCG / Murmur style)
        uint BitHash2D(uint2 p)
        {
            return ((p.x * 1664525u + p.y * 1013904223u + 1376312589u) ^ ((p.x * 747796405u) >> 13u)) & 0x00FFFFFFu;
        }

        // High-frequency procedural micro-grain noise evaluating microscopic crystalline grain boundaries
        float EvaluateMicroGrain(float3 worldPos, float3 T, float3 B)
        {
            // High-frequency brushed lathe coordinates
            float u = dot(worldPos, T) * 45.0f;
            float v = dot(worldPos, B) * 320.0f;

            // Wrap to prevent float precision degradation at large world coordinates
            u = frac(u * 0.05f) * 20.0f;
            v = frac(v * 0.05f) * 20.0f;

            uint2 ip = (uint2)abs(floor(float2(u, v) * 16.0f));
            float2 fp = frac(float2(u, v) * 16.0f);
            fp = fp * fp * (3.0f - 2.0f * fp);

            uint2 p00 = ip;
            uint2 p10 = ip + uint2(1u, 0u);
            uint2 p01 = ip + uint2(0u, 1u);
            uint2 p11 = ip + uint2(1u, 1u);

            float h00 = float(BitHash2D(p00)) / 16777216.0f;
            float h10 = float(BitHash2D(p10)) / 16777216.0f;
            float h01 = float(BitHash2D(p01)) / 16777216.0f;
            float h11 = float(BitHash2D(p11)) / 16777216.0f;

            float h0 = lerp(h00, h10, fp.x);
            float h1 = lerp(h01, h11, fp.x);
            return (lerp(h0, h1, fp.y) * 2.0f - 1.0f);
        }

        float4 PSMain(VertexOutput input) : SV_TARGET
        {
            const float3 N_geom = normalize(input.worldNormal);

            // Unpack dynamic PBR material attributes transmitted via instance buffer
            const float3 albedo    = saturate(input.color.rgb);
            const float  baseRough = clamp(input.materialParams.x, 0.04f, 1.0f);
            const float  metallic  = saturate(input.materialParams.y);
            const float  specular  = saturate(input.materialParams.z);
            const float  ao        = saturate(input.materialParams.w);

            // Construct smooth continuous circumferential lathe tangent frame around the vertical Y axis
            const float lenXZ = length(N_geom.xz);
            const float3 T = (lenXZ > 0.0001f) ? float3(-N_geom.z, 0.0f, N_geom.x) / lenXZ : float3(1.0f, 0.0f, 0.0f);
            const float3 B = cross(N_geom, T);

            // Smoothly fade directional anisotropy and grain at the polar apex where circumferential radius vanishes
            // Eliminates the polar gimbal seam / cap artifact at the crown of the sphere
            const float polarBlend = smoothstep(0.0f, 0.15f, lenXZ);

            // Procedural brushed micro-grain & shimmer (fine-grain crystalline micro-structure for satin titanium)
            const float grainNoise = EvaluateMicroGrain(input.worldPosition, T, B) * metallic * polarBlend;
            const float3 N = normalize(N_geom + B * (grainNoise * 0.025f));

            // Subtle micro-roughness crystalline shimmer
            const float roughness = clamp(baseRough + grainNoise * 0.025f, 0.04f, 1.0f);

            const float cameraDist = length(input.worldPosition);
            const float3 V = (cameraDist > 0.001f) ? (-input.worldPosition / cameraDist) : float3(0.0f, 1.0f, 0.0f);
            const float nDotV = max(dot(N, V), 0.0001f);

            // Pronounced anisotropic highlight aspect for turned/brushed aerospace metals (smoothly isotropic at poles)
            const float aniso = 0.42f * metallic * polarBlend;
            const float anisoAspect = sqrt(1.0f - 0.9f * aniso);
            const float alphaRough = max(roughness * roughness, 0.001f);
            const float at = max(alphaRough / anisoAspect, 0.001f);
            const float ab = max(alphaRough * anisoAspect, 0.001f);

            // Specular reflectance at normal incidence (F0): dielectrics default to 0.08, conductors tinted by albedo
            static const float dielectricBaseF0 = 0.08f;
            const float3 dielectricF0 = float3(dielectricBaseF0, dielectricBaseF0, dielectricBaseF0) * specular;
            const float3 F0 = lerp(dielectricF0, albedo, metallic);

            // Directional albedo and split-sum environment BRDF for the view direction V
            const float2 envBRDF_V = EnvBRDFApprox(roughness, nDotV);
            const float  E_V        = saturate(envBRDF_V.x + envBRDF_V.y);

            // Hemispherical average directional albedo (Fdez-Aguera / Kulla-Conty GGX polynomial fit)
            const float E_avg = saturate(1.0f - 0.34f * roughness - 0.08f * roughness * roughness);
            const float oneMinusE_avg = max(1.0f - E_avg, 0.001f);

            // Average Fresnel reflectance across the hemisphere
            const float3 F_avg = F0 + (float3(1.0f, 1.0f, 1.0f) - F0) * (1.0f / 21.0f);

            // Multi-bounce specular energy conservation ratio representing the infinite geometric series of inter-microfacet bounces
            // Captures inter-microfacet scattering so rough conductors retain their physical warm silver-grey body
            const float3 multiBounceRatio = (F_avg * E_avg) / max(float3(1.0f, 1.0f, 1.0f) - F_avg * (1.0f - E_avg), 0.001f);

            // Specular Image-Based Lighting (IBL) sampled from the 8K Gaia DR3 celestial survey cubemap (register t32)
            // Microfacet roughness maps smoothly across cubemap MIP levels to produce a soft satin aerospace sheen
            const float3 R = reflect(-V, N);
            const float maxMipLevel = 11.0f;
            // Remap roughness deep into the prefiltered MIP hierarchy (MIP 7.5–9.0) for satin aerospace metals,
            // diffusing the galactic core into a soft, uniform starlight wash without prominent bands or hot-spots
            const float mipLevel = clamp(sqrt(roughness) * 4.5f + 6.2f, 7.0f, maxMipLevel);
            const float3 prefilteredColor = g_celestialCube.SampleLevel(g_celestialSampler, R, mipLevel).rgb;

            // Omnidirectional spherical ambient starlight irradiance (MIP 9 captures integrated full-sky cosmic radiance)
            const float3 celestialSkyIrradiance = g_celestialCube.SampleLevel(g_celestialSampler, N, 9.0f).rgb;

            // Neutralise interstellar dust chroma to 100% monochrome starlight so cold titanium retains pure silver spectral reflectance
            const float celestialLuma = dot(prefilteredColor, float3(0.2126f, 0.7152f, 0.0722f));
            const float3 neutralCelestial = float3(celestialLuma, celestialLuma, celestialLuma);

            // Photometric calibration: settle celestial starlight into a grounded ~10-12% ratio relative to direct solar flux
            const float3 cosmicIrradiance = celestialSkyIrradiance * 2.0f + g_ambientColor.rgb * 1.0f;

            // Harmoniously blend directional starlight reflection with omnidirectional background irradiance
            const float3 envRadiance = lerp(cosmicIrradiance, neutralCelestial * 0.28f, 0.20f);

            // Primary single-scattering specular environment reflection
            const float3 singleScatterSpecular = envRadiance * (F0 * envBRDF_V.x + envBRDF_V.y);

            // Fdez-Aguera analytic multi-bounce specular environment term
            const float3 f_ms_IBL = (1.0f - E_V) * multiBounceRatio;
            const float3 multiBounceSpecular = cosmicIrradiance * f_ms_IBL;

            const float3 ambientSpecular = (singleScatterSpecular + multiBounceSpecular) * ao;

            // Diffuse ambient fill for dielectrics (strictly zero for pure metallic conductors)
            const float3 ambientFresnel = FresnelSchlickRoughness(nDotV, F0, roughness);
            const float3 kSAmbient      = ambientFresnel;
            const float3 kDAmbient      = (float3(1.0f, 1.0f, 1.0f) - kSAmbient) * (1.0f - metallic);
            const float3 ambientDiffuse  = (kDAmbient * albedo) * cosmicIrradiance * ao;

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
                    // For directional light (the Sun), evaluate the physical 0.53-degree solar disk core
                    // alongside an expanded forward-scattering aureole modeling diffraction and optical glare
                    const float3 L_core  = (lightType == 0u) ? GetSolarDiskLightDirection(L, V, N, 0.004625f) : L;
                    const float3 L_glare = (lightType == 0u) ? GetSolarDiskLightDirection(L, V, N, 0.048f)     : L;

                    const float3 H_core  = normalize(L_core + V);
                    const float3 H_glare = normalize(L_glare + V);
                    const float hDotV_core  = max(dot(H_core, V), 0.0f);
                    const float hDotV_glare = max(dot(H_glare, V), 0.0f);

                    // 1. Core specular Cook-Torrance anisotropic lobe: razor-sharp glint reflecting base roughness
                    const float NDF_core = DistributionGGXAnisotropic(H_core, T, B, N, at, ab);
                    const float V_core   = VisibilitySmithGGXCorrelatedAnisotropic(V, L_core, T, B, N, at, ab);
                    const float3 F_core  = FresnelSchlick(hDotV_core, F0);

                    // 2. Expansive optical solar glare aureole: forward-scattering micro-haze decoupled from base polish floor
                    // Guarantees an expansive, radiant glow across the surface curvature even at high polish (roughness 0.15)
                    const float at_glare = max(at * 3.5f, 0.28f);
                    const float ab_glare = max(ab * 3.5f, 0.22f);
                    const float NDF_glare = DistributionGGXAnisotropic(H_glare, T, B, N, at_glare, ab_glare);
                    const float V_glare   = VisibilitySmithGGXCorrelatedAnisotropic(V, L_glare, T, B, N, at_glare, ab_glare);
                    const float3 F_glare  = FresnelSchlick(hDotV_glare, F0);

                    // 3. Wide optical flare / lenticular halo envelope (smooth circumsolar forward scatter)
                    const float cosReflection = max(dot(R, L), 0.0f);
                    const float opticalHalo   = pow(cosReflection, 14.0f) * 0.40f;

                    // Composite blazing white-hot solar glint with the broad radiant silver aureole
                    const float3 specularCore  = (NDF_core * V_core) * F_core;
                    const float3 specularGlare = (NDF_glare * V_glare) * F_glare;
                    const float3 specularHalo  = F_glare * opticalHalo;
                    const float3 specularTerm  = specularCore * 1.0f + specularGlare * 1.15f + specularHalo;

                    const float3 F = F_core;

                    // Kulla-Conty direct multi-bounce specular energy conservation
                    // Evaluates inter-microfacet scattering for incident light direction L
                    float2 envBRDF_L = EnvBRDFApprox(roughness, nDotL);
                    float  E_L       = saturate(envBRDF_L.x + envBRDF_L.y);

                    // Analytic multi-bounce specular lobe: reciprocal, energy-conserving, tinted by metal reflectance F0
                    // Balances body reflectance with crisp specular contrast so highlights pop with lustrous metallic shine
                    float3 f_ms_direct = ((1.0f - E_V) * (1.0f - E_L) / (PI * oneMinusE_avg)) * multiBounceRatio * 2.2f;

                    // Strict energy conservation: conductors possess zero internal diffuse reflection
                    float3 kS = F;
                    float3 kD = (float3(1.0f, 1.0f, 1.0f) - kS) * (1.0f - metallic);

                    float3 directDiffuse  = (kD * albedo) / PI;
                    float3 directSpecular = (specularTerm + f_ms_direct) * radiance * nDotL;
                    float3 directLighting = directDiffuse * radiance * nDotL + directSpecular;

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

            return float4(max(shadedColor, 0.0f), input.color.a);
        }
    )";

    inline constexpr const char* s_embeddedUnlitPixelStage = R"(
        float4 PSMain(VertexOutput input) : SV_TARGET
        {
            return input.color;
        }
    )";

    inline constexpr const char* s_embeddedDepthOnlyPassVertexStage = R"(
        struct VertexInput
        {
            float3 position : POSITION;
            float3 normal   : NORMAL;
            float4 color    : COLOR;
        };

        float4 VSMain(VertexInput input, uint instanceId : SV_InstanceID) : SV_POSITION
        {
            if (g_isInstanced != 0)
            {
                InstanceData inst = g_instances[instanceId];
                return mul(float4(input.position, 1.0f), inst.mvp);
            }
            else
            {
                return mul(float4(input.position, 1.0f), g_mvp);
            }
        }
    )";

    inline constexpr const char* s_embeddedPostProcessVertexStage = R"(
        struct PostProcessVertexOutput
        {
            float4 position : SV_POSITION;
            float2 uv       : TEXCOORD0;
        };

        PostProcessVertexOutput VSMain(uint vertexId : SV_VertexID)
        {
            PostProcessVertexOutput output;
            float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
            output.uv = uv;
            output.position = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
            return output;
        }
    )";

    inline constexpr const char* s_embeddedPostProcessPixelStage = R"(
        Texture2D g_hdrSceneTexture : register(t33);
        SamplerState g_samplerLinear : register(s1);

        struct PostProcessVertexOutput
        {
            float4 position : SV_POSITION;
            float2 uv       : TEXCOORD0;
        };

        float4 PSMain(PostProcessVertexOutput input) : SV_TARGET
        {
            const float4 hdrSample = g_hdrSceneTexture.Sample(g_samplerLinear, input.uv);
            const float3 hdrColor = max(hdrSample.rgb, 0.0f);

            const float exposure = max(g_exposureParams.x, 0.0001f);
            const float3 exposedRadiance = hdrColor * exposure;
            const float3 tonemappedRadiance = AcesFilmicToneMapping(exposedRadiance);

            const float ditherSeedA = frac(sin(dot(input.position.xy, float2(12.9898f, 78.233f))) * 43758.5453f);
            const float ditherSeedB = frac(sin(dot(input.position.xy + float2(1.0f, 1.0f), float2(12.9898f, 78.233f))) * 43758.5453f);
            const float dither = (ditherSeedA + ditherSeedB - 1.0f) * (0.5f / 255.0f);
            const float hasSignal = step(0.0001f, dot(tonemappedRadiance, float3(1.0f, 1.0f, 1.0f)));

            return float4(saturate(tonemappedRadiance + dither * hasSignal), hdrSample.a);
        }
    )";
}

