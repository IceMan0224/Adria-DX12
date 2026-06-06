#include "Lighting.hlsli"
#include "Packing.hlsli"

#define BLOCK_SIZE 16

struct DeferredLightingConstants
{
	uint normalIdx;
	uint diffuseIdx;
	uint emissiveIdx;
	uint customIdx;
	uint depthIdx;
	uint aoIdx;
	uint outputIdx;
};
DECLARE_CBUFFER(DeferredLightingConstants, DeferredLightingPassCB, 1);

struct DeferredLightingRestirConstants
{
	uint restirDIOutputIdx;
	uint restirGIOutputIdx;
};
DECLARE_CBUFFER(DeferredLightingRestirConstants, DeferredLightingPassCB2, 2);

// Direct lighting: ReSTIR DI output if available, otherwise the analytic per-light loop.
float3 ComputeDirectLighting(uint shadingExtension, BrdfData brdfData, float3 viewPosition, float3 viewNormal, float3 V, float2 uv, float4 customData, uint2 pixel)
{
	uint restirDIOutputIdx = DeferredLightingPassCB2.restirDIOutputIdx;
	if (restirDIOutputIdx != 0xFFFFFFFF)
	{
		// ReSTIR DI handles all light types (point, spot, directional)
		Texture2D<float4> restirDIOutput = ResourceDescriptorHeap[restirDIOutputIdx];
		return restirDIOutput[pixel].rgb;
	}

	float3 directLighting = 0.0f;
	for (uint i = 0; i < FrameCB.lightCount; ++i)
	{
		LightInfo lightInfo = LoadLightInfo(i);
		if (!lightInfo.active) continue;
		directLighting += DoLight(shadingExtension, lightInfo, brdfData, viewPosition, viewNormal, V, uv, customData);
	}
	return directLighting;
}

// Indirect lighting: dispatched through GetIndirectLighting, which handles ReSTIR GI / DDGI / ambient.
float3 ComputeIndirectLighting(float3 viewPosition, float3 viewNormal, float3 diffuseColor, float ambientOcclusion, uint2 pixel)
{
	return GetIndirectLighting(viewPosition, viewNormal, diffuseColor, ambientOcclusion, DeferredLightingPassCB2.restirGIOutputIdx, pixel);
}

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint  GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void DeferredLightingCS(CSInput input)
{
	Texture2D               normalRT			  = ResourceDescriptorHeap[DeferredLightingPassCB.normalIdx];
	Texture2D               diffuseRT			  = ResourceDescriptorHeap[DeferredLightingPassCB.diffuseIdx];
	Texture2D				emissiveRT			  = ResourceDescriptorHeap[DeferredLightingPassCB.emissiveIdx];
	Texture2D               customRT			  = ResourceDescriptorHeap[DeferredLightingPassCB.customIdx];
	Texture2D<float>        depthTexture		  = ResourceDescriptorHeap[DeferredLightingPassCB.depthIdx];
	Texture2D<float>		ambientOcclusionTexture = ResourceDescriptorHeap[DeferredLightingPassCB.aoIdx];
	
	float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.renderResolution);

	float3 viewNormal;
	float metallic;
	uint  shadingExtension;
	float4 normalRTData = normalRT.SampleLevel(LinearWrapSampler, uv, 0);
	DecodeGBufferNormalRT(normalRTData, viewNormal, metallic, shadingExtension);
	float  depth		  = depthTexture.SampleLevel(LinearWrapSampler, uv, 0);

	float3 viewPosition		= GetViewPosition(uv, depth);
	float3 V				= normalize(float3(0.0f, 0.0f, 0.0f) - viewPosition);

	float4 albedoRoughness	= diffuseRT.SampleLevel(LinearWrapSampler, uv, 0);
	float3 albedo			= albedoRoughness.rgb;
	float  roughness		= albedoRoughness.a;
	float4 customData       = customRT.SampleLevel(LinearWrapSampler, uv, 0);
	
	BrdfData brdfData = GetBrdfData(albedo, metallic, roughness);

	uint2 pixel = input.DispatchThreadId.xy;
	float ambientOcclusion = ambientOcclusionTexture.SampleLevel(LinearWrapSampler, uv, 0);

	float3 directLighting   = ComputeDirectLighting(shadingExtension, brdfData, viewPosition, viewNormal, V, uv, customData, pixel);
	float3 indirectLighting = ComputeIndirectLighting(viewPosition, viewNormal, brdfData.Diffuse, ambientOcclusion, pixel);

	float4 emissiveData = emissiveRT.SampleLevel(LinearWrapSampler, uv, 0);
	float3 emissiveColor = emissiveData.rgb * emissiveData.a * 256;

	RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[DeferredLightingPassCB.outputIdx];
	outputTexture[pixel] = float4(indirectLighting + directLighting + emissiveColor, 1.0f);
}