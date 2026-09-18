// Copyright © 2026 spacegirl65. All Rights Reserved.

struct LightData
{
    float4 position;    // xyz = world position, w = range
    float4 direction;   // xyz = normalized direction, w = type (0=Dir, 1=Point, 2=Spot)
    float4 color;       // rgb = light color, w = intensity
    float4 attenuation; // x = constant, y = linear, z = quadratic, w = inner/outer spot cosine
};

cbuffer SceneConstantBuffer : register(b0)
{
    row_major float4x4 g_mvp;
    row_major float4x4 g_world;
    float4             g_ambientColor;
    uint               g_lightCount;
    uint3              g_lightPadding;
    LightData          g_lights[16];
};

struct PixelInput
{
    float4 position      : SV_POSITION;
    float3 worldNormal   : NORMAL;
    float4 color         : COLOR;
    float3 worldPosition : TEXCOORD0;
};

float4 PSMain(PixelInput input) : SV_TARGET
{
    float3 N = normalize(input.worldNormal);
    float3 ambient = g_ambientColor.rgb;
    float3 totalDiffuse = float3(0.0f, 0.0f, 0.0f);

    const uint activeLightCount = min(g_lightCount, 16u);

    for (uint i = 0; i < activeLightCount; ++i)
    {
        LightData light = g_lights[i];
        uint lightType = (uint)light.direction.w;
        float intensity = light.color.w;
        float3 lightRgb = light.color.rgb * intensity;

        if (lightType == 0u) // Directional Light
        {
            float3 L = normalize(-light.direction.xyz);
            float nDotL = max(dot(N, L), 0.0f);
            totalDiffuse += lightRgb * nDotL;
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

                totalDiffuse += lightRgb * (nDotL * att * falloff);
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

                totalDiffuse += lightRgb * (nDotL * att * falloff * spotFactor);
            }
        }
    }

    float3 shadedColor = input.color.rgb * (ambient + totalDiffuse);
    return float4(shadedColor, input.color.a);
}

