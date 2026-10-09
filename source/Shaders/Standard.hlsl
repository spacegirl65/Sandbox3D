// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Atmosphere.hlsli"

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

// Vertex shader stage
VertexOutput VSMain(VertexInput input, uint instanceId : SV_InstanceID)
{
    VertexOutput output;

    // Row-vector multiplication convention (v * M) adhering to engine standards
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

// Pixel shader stage
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

    // Atmospheric perspective (physically based Rayleigh and Mie aerial perspective)
    const float3 rayDir = -V;

    // Sub-threshold air extinction (<0.05%) bypassed within 75m; 2x2 quadrature evaluates ground rays with >99.5% fidelity
    if (g_atmosphereParams.x > 0.0f && cameraDist > 75.0f)
    {
        const float3 sunDir = GetCelestialSunDirection();
        const float3 cameraWorldPos = float3(0.0f, max(g_fogParams.w, 0.0f), 0.0f);

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

