// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "Atmosphere.hlsli"

struct SkyVertexOutput
{
    float4 position : SV_POSITION;
    float3 viewDir  : TEXCOORD0;
};

// Full-screen triangle covering the screen viewport at Reverse-Z depth 0.0 (infinite distance)
SkyVertexOutput VSMain(uint vertexId : SV_VertexID)
{
    SkyVertexOutput output;

    // Generates a full-screen triangle covering [-1, 1] in clip space
    // Vertex 0: (-1, -1), Vertex 1: (-1, 3), Vertex 2: (3, -1)
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    float2 clipPos = uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);

    // In a Reverse-Z projection pipeline, 0.0f represents infinite distance (far clip plane)
    output.position = float4(clipPos, 0.0f, 1.0f);

    // Unproject the clip position into camera view space using the inverse projection matrix
    // In row-vector convention (v * M), unprojected = float4(clipPos.x, clipPos.y, 0.0f, 1.0f) * InvViewProj
    // For standard perspective:
    // P_00 = 1 / (aspect * tan(fov/2)) = g_mvp column scale, P_11 = 1 / tan(fov/2)
    // View vector in view-space: (clipPos.x / P_00, clipPos.y / P_11, 1.0)
    // Rotating by inverse view rotation yields world-space direction
    float invProjX = 1.0f / max(g_mvp._m00, 0.0001f);
    float invProjY = 1.0f / max(g_mvp._m11, 0.0001f);
    float3 viewRayCamera = float3(clipPos.x * invProjX, clipPos.y * invProjY, 1.0f);

    // Rotate view ray from camera space into world space using the camera's world orientation
    // Camera world basis vectors are stored in g_world (or transposed view rotation)
    // With camera-relative world convention:
    output.viewDir = mul(float4(viewRayCamera, 0.0f), g_world).xyz;

    return output;
}

// Pixel shader evaluating physical single-scattering sky dome and solar disc
float4 PSMain(SkyVertexOutput input) : SV_TARGET
{
    // Reconstruct normalized celestial view direction
    const float3 viewDir = normalize(input.viewDir);

    // Clamp horizon elevation angle for sky dome evaluation so the boundary smoothly meets the landscape
    float3 domeViewDir = viewDir;
    domeViewDir.y = max(domeViewDir.y, 0.002f);
    domeViewDir = normalize(domeViewDir);

    // Identify primary celestial sun orientation
    const float3 sunDir = GetCelestialSunDirection();

    // Camera world position relative to planetary surface center
    const float3 cameraWorldPos = float3(0.0f, max(g_world._m31, 0.0f), 0.0f);

    // Evaluate physical single scattering atmospheric radiance (Rayleigh, Mie, and ozone)
    const float3 skyRadiance = EvaluateSkyRadiance(cameraWorldPos, domeViewDir, sunDir, 16);

    // Apply exposure and ACES filmic tone reproduction directly before MSAA resolve
    const float exposure = max(g_exposureParams.x, 0.0001f);
    const float3 exposedRadiance = skyRadiance * exposure;
    const float3 tonemappedRadiance = AcesFilmicToneMapping(exposedRadiance);

    return float4(tonemappedRadiance, 1.0f);
}

