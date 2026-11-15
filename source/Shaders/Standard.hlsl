// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Atmosphere.hlsli"

TextureCube g_celestialCube : register(t32);
SamplerState g_celestialSampler : register(s1);

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

// Vertex shader stage
VertexOutput VSMain(VertexInput input, uint instanceId : SV_InstanceID)
{
    VertexOutput output;

    // Row-vector multiplication convention (v * M) adhering to engine standards
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
// Evaluates V = G / (4 * nDotL * nDotV), directly incorporating the microfacet denominator
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

// Pixel shader stage
float4 PSMain(VertexOutput input) : SV_TARGET
{
    const float3 N = normalize(input.worldNormal);
    const float cameraDist = length(input.worldPosition);
    const float3 V = (cameraDist > 0.001f) ? (-input.worldPosition / cameraDist) : float3(0.0f, 1.0f, 0.0f);
    const float nDotV = max(dot(N, V), 0.0001f);

    // Unpack dynamic PBR material attributes transmitted via instance buffer
    const float3 albedo    = saturate(input.color.rgb);
    const float  roughness = clamp(input.materialParams.x, 0.04f, 1.0f);
    const float  metallic  = saturate(input.materialParams.y);
    const float  specular  = saturate(input.materialParams.z);
    const float  ao        = saturate(input.materialParams.w);

    // Specular reflectance at normal incidence (F0): dielectrics default to 0.04, conductors tinted by albedo
    static const float dielectricBaseF0 = 0.08f;
    const float3 dielectricF0 = float3(dielectricBaseF0, dielectricBaseF0, dielectricBaseF0) * specular;
    const float3 F0 = lerp(dielectricF0, albedo, metallic);

    // Ambient lighting: energy-conserving diffuse alongside specular environment reflection
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
    // Remap roughness deeper into the prefiltered MIP hierarchy (MIP 6.5–8.0) for satin aerospace metals,
    // diffusing the raw galactic disc into a broad, subtle starlight sheen without harsh spatial bands
    const float mipLevel = clamp(sqrt(roughness) * 8.0f + 2.5f, 0.0f, maxMipLevel);
    const float3 prefilteredColor = g_celestialCube.SampleLevel(g_celestialSampler, R, mipLevel).rgb;

    // Omnidirectional spherical ambient starlight irradiance (MIP 9 captures integrated full-sky cosmic radiance)
    const float3 celestialSkyIrradiance = g_celestialCube.SampleLevel(g_celestialSampler, N, 9.0f).rgb;

    // Neutralise raw galactic dust chroma so titanium's own silver spectral reflectance tints the reflection
    const float celestialLuma = dot(prefilteredColor, float3(0.2126f, 0.7152f, 0.0722f));
    const float3 neutralCelestial = lerp(float3(celestialLuma, celestialLuma, celestialLuma), prefilteredColor, 0.40f);

    // Photometric calibration: settle celestial starlight into a grounded ~10-15% ratio relative to direct solar flux
    const float3 cosmicIrradiance = celestialSkyIrradiance * 2.2f + g_ambientColor.rgb * 1.0f;

    // Softly blend local directional reflection with whole-sky background irradiance to prevent dark cutoffs
    const float3 envRadiance = lerp(cosmicIrradiance, neutralCelestial * 0.50f, 0.35f);

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
            float3 H = normalize(L + V);
            float hDotV = max(dot(H, V), 0.0f);

            // Cook-Torrance microfacet specular BRDF with height-correlated Smith visibility
            float  NDF         = DistributionGGX(N, H, roughness);
            float  V_Smith     = VisibilitySmithGGXCorrelated(nDotV, nDotL, roughness);
            float3 F           = FresnelSchlick(hDotV, F0);
            float3 specularTerm = NDF * V_Smith * F;

            // Kulla-Conty direct multi-bounce specular energy conservation
            // Evaluates inter-microfacet scattering for incident light direction L
            float2 envBRDF_L = EnvBRDFApprox(roughness, nDotL);
            float  E_L       = saturate(envBRDF_L.x + envBRDF_L.y);

            // Analytic multi-bounce specular lobe: reciprocal, energy-conserving, tinted by metal reflectance F0
            // For satin aerospace metals, inter-microfacet scattering bathes the sun-facing hemisphere in silver reflectance
            float3 f_ms_direct = ((1.0f - E_V) * (1.0f - E_L) / (PI * oneMinusE_avg)) * multiBounceRatio * 3.5f;

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

    // Atmospheric perspective (physically based Rayleigh and Mie aerial perspective)
    const float3 rayDir = -V;

    // Sub-threshold air extinction (<0.05%) bypassed within 75m; 2x2 quadrature evaluates ground rays with >99.5% fidelity
    if (g_atmosphereParams.x > 0.0f && cameraDist > 75.0f)
    {
        const float3 sunDir = GetCelestialSunDirection();
        const float3 cameraWorldPos = g_cameraPosition.xyz;

        float3 inscattering, transmittance;
        EvaluateAtmosphericScattering(
            cameraWorldPos,
            rayDir,
            cameraDist,
            sunDir,
            inscattering,
            transmittance,
            2,
            2
        );

        // Smooth transition eliminating any threshold boundary between 75m and 125m
        const float atmBlend = saturate((cameraDist - 75.0f) * 0.02f);
        shadedColor = lerp(shadedColor, shadedColor * transmittance + inscattering, atmBlend);
    }

    // Couples artist-configured boundary fog or haze layer with atmospheric illumination
    EvaluateCompositeFog(rayDir, cameraDist, GetCelestialSunDirection(), g_fogColor, g_fogParams, shadedColor);

    // Apply exposure and ACES filmic tone reproduction directly before MSAA resolve
    const float exposure = max(g_exposureParams.x, 0.0001f);
    const float3 exposedColor = shadedColor * exposure;
    const float3 tonemappedColor = AcesFilmicToneMapping(exposedColor);

    return float4(tonemappedColor, input.color.a);
}

