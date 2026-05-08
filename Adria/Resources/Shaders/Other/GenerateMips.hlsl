#include "CommonResources.hlsli"

Texture2D<float4> SrcTexture : register(t0);
RWTexture2D<float4> DstTexture : register(u0);

struct GenerateMipsConstants
{
	float2 texelSize;
	uint   srcIdx;
	uint   dstIdx;
};
DECLARE_CBUFFER(GenerateMipsConstants, PassCB, 1);

[numthreads(8, 8, 1)]
void GenerateMipsCS(uint3 DTid : SV_DispatchThreadID)
{
	Texture2D<float4> SrcTexture = ResourceDescriptorHeap[PassCB.srcIdx];
	RWTexture2D<float4> DstTexture = ResourceDescriptorHeap[PassCB.dstIdx];

	float2 uv = (DTid.xy + 0.5) * PassCB.texelSize;
	DstTexture[DTid.xy] = SrcTexture.SampleLevel(LinearClampSampler, uv, 0);
}

