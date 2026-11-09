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

    // Identify primary celestial sun orientation
    const float3 sunDir = GetCelestialSunDirection();

    // Clamp horizon elevation angle for sky dome evaluation so the boundary smoothly meets the landscape
    // When looking below the horizon, smoothly fade directional azimuth into a neutral horizon reference
    // so that views directed downwards towards the ground or nadir are completely uniform with zero radial pinwheels.
    // Preserve natural view azimuth across the horizon to maintain warm atmospheric scattering and prevent artificial colour seams behind terrain.
    // Scale the minimum elevation threshold by horizontal length (lenXZ) so vectors pointing downwards towards the nadir
    // remain on the horizon rim rather than inverting into the dark zenith directly overhead.
    // Camera world position relative to planetary surface center (ensuring altitude is safely above planet datum)
    const float cameraAltitude = max(g_cameraPosition.y, 0.0f) + 1.0f;
    const float3 cameraWorldPos = float3(0.0f, cameraAltitude, 0.0f);

    // Preserve authentic view direction across the entire upper celestial hemisphere (elevation >= 0.002).
    // Clamping elevation smoothly to the horizon rim only when viewing below the horizon preserves physical Rayleigh/Mie scattering
    // without distorting zenith angles or inducing radial fan singularities overhead.
    const float lenXZ = length(viewDir.xz);
    const float3 domeViewDir = (viewDir.y >= 0.002f)
        ? viewDir
        : ((lenXZ > 0.0001f) ? normalize(float3(viewDir.x, 0.002f, viewDir.z)) : float3(0.0f, 1.0f, 0.0f));

    // Evaluate physical single scattering atmospheric radiance (Rayleigh, Mie, and ozone)
    float3 skyRadiance = EvaluateSkyRadiance(cameraWorldPos, domeViewDir, sunDir, 16);

    // When looking downwards into the planet/ground (below the horizon), smoothly dissolve directional
    // horizon radiance into a uniform nadir ambient tone.
    // Blending radiance in colour space rather than rotating azimuth preserves authentic horizon skyline colours
    // while completely eliminating polar pinwheels and radial fan singularities at the nadir.
    if (viewDir.y < -0.02f)
    {
        // Neutral reference radiance evaluated orthogonal to the sun along the horizon
        const float3 orthoSun = cross(sunDir, float3(0.0f, 1.0f, 0.0f));
        const float lenOrtho = length(orthoSun.xz);
        const float2 neutralAzimuth = (lenOrtho > 0.0001f) ? (orthoSun.xz / lenOrtho) : float2(1.0f, 0.0f);
        const float3 neutralHorizonDir = normalize(float3(neutralAzimuth.x, 0.002f, neutralAzimuth.y));
        const float3 neutralHorizonRadiance = EvaluateSkyRadiance(cameraWorldPos, neutralHorizonDir, sunDir, 8);

        const float3 nadirRadiance = lerp(neutralHorizonRadiance, g_ambientColor.rgb * 0.35f, 0.50f);

        // Smooth cubic transition commencing below mountain ridges (-0.02) and completing towards the nadir (-0.45)
        const float nadirBlend = smoothstep(0.0f, 1.0f, saturate((-viewDir.y - 0.02f) / 0.43f));
        skyRadiance = lerp(skyRadiance, nadirRadiance, nadirBlend);
    }

    // Apply exposure and ACES filmic tone reproduction directly before MSAA resolve
    const float exposure = max(g_exposureParams.x, 0.0001f);
    const float3 exposedRadiance = skyRadiance * exposure;
    const float3 tonemappedRadiance = AcesFilmicToneMapping(exposedRadiance);

    return float4(tonemappedRadiance, 1.0f);
}

