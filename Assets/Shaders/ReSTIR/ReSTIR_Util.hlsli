#include "Random.hlsli"
#include "Tonemapping.hlsli"
#include "Surface.hlsli"
#include "LightInfo.hlsli"
#include "Lighting.hlsli"

static const uint ReSTIR_InvalidLightIndex = uint(-1);

float2 ReSTIR_RandomlySelectInfiniteLightUV(inout RNG rng)
{
    float2 uv;
    uv.x = RNG_GetNext(rng);
    uv.y = RNG_GetNext(rng);
    return uv;
}

float2 ReSTIR_RandomlySelectLocalLightUV(inout RNG rng)
{
    float2 uv;
    uv.x = RNG_GetNext(rng);
    uv.y = RNG_GetNext(rng);
    return uv;
}

LightInfo ReSTIR_LoadLightInfoWS(uint lightIndex)
{
    LightInfo lightInfo = LoadLightInfo(lightIndex);
    lightInfo.position = mul(float4(lightInfo.position.xyz, 1.0f), FrameCB.inverseView);
    lightInfo.position.xyz /= lightInfo.position.w;
    lightInfo.direction.xyz = mul(lightInfo.direction.xyz, (float3x3)FrameCB.inverseView);
    return lightInfo;
}

//`lightInfoWS` must already be in world space.
float3 ReSTIR_ShadeSurfaceWithLightSample(LightSample lightSample, LightInfo lightInfoWS, Surface surface)
{
    float3 toLight = lightSample.position - surface.worldPos;
    float3 L = normalize(toLight);

    float NdotL = saturate(dot(L, surface.worldNormal));
    if (NdotL <= 0)
        return 0;

    float distance = length(toLight);
    float attenuation = 1.0f;
    if (lightInfoWS.type == POINT_LIGHT)
    {
        attenuation = DoAttenuation(distance, lightInfoWS.range);
    }
    else if (lightInfoWS.type == SPOT_LIGHT)
    {
        attenuation = DoAttenuation(distance, lightInfoWS.range);
        float3 spotDir = normalize(lightInfoWS.direction.xyz);
        float cosAng = dot(-spotDir, L);
        float conAtt = saturate((cosAng - lightInfoWS.outerCosine) / (lightInfoWS.innerCosine - lightInfoWS.outerCosine));
        conAtt *= conAtt;
        attenuation *= conAtt;
    }
    if (attenuation <= 0)
        return 0;

    float3 brdf = DefaultBRDF(L, surface.viewDir, surface.worldNormal,
                              surface.brdfData.Diffuse, surface.brdfData.Specular, surface.brdfData.Roughness);
    return brdf * NdotL * attenuation * lightInfoWS.color.rgb;
}

float ReSTIR_GetLightSampleTargetPdfForSurface(LightSample lightSample, LightInfo lightInfoWS, Surface surface)
{
    return CalculateLuminance(ReSTIR_ShadeSurfaceWithLightSample(lightSample, lightInfoWS, surface));
}

// `lightInfoWS` must already be in world space.
LightSample ReSTIR_SampleLight(LightInfo lightInfoWS, Surface surface, float2 uv)
{
    return CalculateLightSample(lightInfoWS, uv, surface.worldPos);
}