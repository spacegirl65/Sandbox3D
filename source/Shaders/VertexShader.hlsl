// Copyright © 2026 spacegirl65. All Rights Reserved.

cbuffer ModelViewProjectionBuffer : register(b0)
{
    row_major float4x4 g_mvp;
};

struct VertexInput
{
    float3 position : POSITION;
    float4 color    : COLOR;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

VertexOutput VSMain(VertexInput input)
{
    VertexOutput output;

    // Row-vector multiplication convention (v * M) adhering to engine standards
    output.position = mul(float4(input.position, 1.0f), g_mvp);
    output.color = input.color;

    return output;
}

