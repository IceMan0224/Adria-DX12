#include "RayTracingUtil.hlsli"
#include "Packing.hlsli"

struct RayTracedAmbientOcclusionConstants
{
	uint  depthIdx;
	uint  normalsIdx;
	uint  outputIdx;
	float aoRadius;
	float aoPower;
};
DECLARE_CBUFFER(RayTracedAmbientOcclusionConstants, RayTracedAmbientOcclusionPassCB, 1);

[numthreads(16, 16, 1)]
void RayTracedAmbientOcclusionCS(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	uint2 launchIndex = dispatchThreadId.xy;
	if (launchIndex.x >= (uint)FrameCB.renderResolution.x || launchIndex.y >= (uint)FrameCB.renderResolution.y)
		return;

	Texture2D<float>   depthTexture  = ResourceDescriptorHeap[RayTracedAmbientOcclusionPassCB.depthIdx];
	Texture2D          normalRT      = ResourceDescriptorHeap[RayTracedAmbientOcclusionPassCB.normalsIdx];
	RWTexture2D<float> outputTexture = ResourceDescriptorHeap[RayTracedAmbientOcclusionPassCB.outputIdx];

	float depth = depthTexture.Load(int3(launchIndex, 0)).r;
	float2 texCoords = (launchIndex + 0.5f) / FrameCB.renderResolution;
	float3 worldPosition = GetWorldPosition(texCoords, depth);
	float3 viewNormal = DecodeNormalOctahedron(normalRT.Load(int3(launchIndex, 0)).xy * 2.0f - 1.0f);
	float3 worldNormal = normalize(mul(viewNormal, (float3x3) FrameCB.inverseView));

	uint2 launchDim = (uint2)FrameCB.renderResolution;
	RNG rng = RNG_Initialize(launchIndex.x + launchIndex.y * launchDim.x, 47, 16);
	float2 offset = float2(RNG_GetNext(rng), RNG_GetNext(rng));
	float3 worldDir = GetCosHemisphereSample(rng, worldNormal);

	RayDesc ray;
	ray.Origin    = OffsetRay(worldPosition, worldNormal);
	ray.Direction = normalize(worldDir);
	ray.TMin      = 0.02f;
	ray.TMax      = RayTracedAmbientOcclusionPassCB.aoRadius;

	HitInfo hitInfo;
	bool hit = TraceRay(ray, hitInfo);
	float tHit = hit ? hitInfo.hitT : -1.0f;
	outputTexture[launchIndex] = tHit < 0.0f ? 1.0f : pow(saturate(tHit / RayTracedAmbientOcclusionPassCB.aoRadius), RayTracedAmbientOcclusionPassCB.aoPower);
}
