// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "SceneBuffers.hlsli"

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
VertexOutput VSMain(VertexInput input)
{
    VertexOutput output;

    // Row-vector multiplication convention (v * M) adhering to engine standards
    output.position      = mul(float4(input.position, 1.0f), g_mvp);
    output.worldNormal   = normalize(mul(float4(input.normal, 0.0f), g_world).xyz);
    output.worldPosition = mul(float4(input.position, 1.0f), g_world).xyz;
    output.color         = input.color;

    return output;
}

// Pixel shader stage
float4 PSMain(VertexOutput input) : SV_TARGET
{
    const float3 N = normalize(input.worldNormal);
    const float cameraDist = length(input.worldPosition);
    const float3 V = (cameraDist > 0.001f) ? (-input.worldPosition / cameraDist) : float3(0.0f, 1.0f, 0.0f);

    // Standard surface specular reflectance characteristics
    static const float specPower     = 32.0f;
    static const float specIntensity = 0.25f;

    float3 ambient       = g_ambientColor.rgb;
    float3 totalDiffuse  = float3(0.0f, 0.0f, 0.0f);
    float3 totalSpecular = float3(0.0f, 0.0f, 0.0f);

    const uint activeLightCount = min(g_lightCount, 16u);

    for (uint i = 0; i < activeLightCount; ++i)
    {
        LightData light = g_lights[i];
        uint lightType  = (uint)light.direction.w;
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
            float dist     = length(toLight);
            float range    = max(light.position.w, 0.001f);

            if (dist < range)
            {
                float3 L = toLight / dist;
                float nDotL = max(dot(N, L), 0.0f);

                // Quadratic attenuation with smooth quadratic range windowing
                float att     = 1.0f / (light.attenuation.x + light.attenuation.y * dist + light.attenuation.z * dist * dist);
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
            float dist     = length(toLight);
            float range    = max(light.position.w, 0.001f);

            if (dist < range)
            {
                float3 L = toLight / dist;
                float nDotL = max(dot(N, L), 0.0f);

                // Spot cone factor (attenuation.z = inner cone cos, attenuation.w = outer cone cos)
                float cosAngle   = dot(-L, normalize(light.direction.xyz));
                float innerCos   = light.attenuation.z;
                float outerCos   = light.attenuation.w;
                float spotFactor = saturate((cosAngle - outerCos) / max(innerCos - outerCos, 0.0001f));

                float att     = 1.0f / (light.attenuation.x + light.attenuation.y * dist + light.attenuation.z * dist * dist);
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
        const float fogExtent    = max(cameraDist - g_fogParams.x, 0.0f);
        const float opticalDepth = fogExtent * g_fogParams.z;
        // Exponential squared optical depth formulation
        const float fogFactor    = saturate((1.0f - exp(-opticalDepth * opticalDepth)) * g_fogColor.a);
        shadedColor = lerp(shadedColor, g_fogColor.rgb, fogFactor);
    }

    return float4(shadedColor, input.color.a);
}

