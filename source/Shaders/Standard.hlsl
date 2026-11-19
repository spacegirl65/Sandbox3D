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

// Pixel shader stage
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
            // For directional light (the Sun), evaluate the distance-scaled solar photosphere disc
            // alongside an expanded forward-scattering aureole modelling diffraction and optical glare
            const float minSolarDistanceAU    = 0.001f;
            const float solarAngularRadius1AU = 0.00465047f;
            const float glareAngularRadius    = 0.048f;
            const float solarDistAU           = (g_adaptationParams.z > 0.0001f) ? max(g_adaptationParams.z, minSolarDistanceAU) : 1.0f;
            const float sunAngularRadius      = solarAngularRadius1AU / solarDistAU;

            const float3 L_core  = (lightType == 0u) ? GetSolarDiskLightDirection(L, V, N, sunAngularRadius)   : L;
            const float3 L_glare = (lightType == 0u) ? GetSolarDiskLightDirection(L, V, N, glareAngularRadius) : L;

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

