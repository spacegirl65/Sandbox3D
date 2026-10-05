// Copyright © 2026 spacegirl65. All Rights Reserved.

#include "SceneBuffers.hlsli"

struct VertexInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 color    : COLOR;
};

// Row-major matrix transformation evaluating homogeneous light-space clip position
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

