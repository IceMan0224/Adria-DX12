#include "LightInfo.hlsli"
#include "RayTracingUtil.hlsli"

struct RayTracedShadowsConstants
{
	uint  depthIdx;
	uint  lightIdx;
};
DECLARE_CBUFFER(RayTracedShadowsConstants, RayTracedShadowsPassCB, 1);

[numthreads(16, 16, 1)]
void RayTracedShadowsCS(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	uint2 launchIndex = dispatchThreadId.xy;
	if (launchIndex.x >= (uint)FrameCB.renderResolution.x || launchIndex.y >= (uint)FrameCB.renderResolution.y)
		return;

	Texture2D<float> depthTexture = ResourceDescriptorHeap[RayTracedShadowsPassCB.depthIdx];
	LightInfo lightInfo = LoadLightInfo(RayTracedShadowsPassCB.lightIdx);
	RWTexture2D<float> outputTexture = ResourceDescriptorHeap[lightInfo.shadowMaskIndex];

	float depth = depthTexture.Load(int3(launchIndex, 0)).r;
	float2 texCoords = (launchIndex + 0.5f) / FrameCB.renderResolution;
	float3 worldPos = GetWorldPosition(texCoords, depth);

	bool lit = TraceShadowRay(lightInfo, worldPos, FrameCB.inverseView);
	outputTexture[launchIndex] = lit ? 1.0f : 0.0f;
}
