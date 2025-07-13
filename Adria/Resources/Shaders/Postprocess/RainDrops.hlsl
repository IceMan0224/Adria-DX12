#include "CommonResources.hlsli"

#define BLOCK_SIZE 16

struct RainDropsConstants
{
	uint   outputIdx;
	uint   noiseIdx;
};
ConstantBuffer<RainDropsConstants> RainDropsPassCB : register(b1);

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint GroupIndex : SV_GroupIndex;
};

//credit: https://www.shadertoy.com/view/ldSBWW
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void RainDropsCS(CSInput input)
{
	RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[RainDropsPassCB.outputIdx];
	Texture2D<float4> noiseTexture = ResourceDescriptorHeap[RainDropsPassCB.noiseIdx];

	uint2 pixelCoord = input.DispatchThreadId.xy;
	float2 resolution = FrameCB.renderResolution;
	float2 uv = (float2(pixelCoord) + 0.5f) / resolution;
	float2 n = noiseTexture.SampleLevel(LinearWrapSampler, uv * 0.1f, 0).rg;

	float3 result = 0.0f;
	for (float r = 4.0f; r > 0.0f; r -= 1.0f)
	{
		float2 dropGrid = resolution * r * 0.015f;
		float2 p = 6.2831f * uv * dropGrid + (n - 0.5f) * 2.0f;
		float2 s = sin(p);

		float2 coord = round(uv * dropGrid - 0.25f) / dropGrid;
		float4 d = noiseTexture.SampleLevel(LinearWrapSampler, coord, 0);

		float t = (s.x + s.y) * max(0.0f, 1.0f - frac(FrameCB.rainTotalTime * (d.b + 0.1f) + d.g) * 2.0f);

		if (d.r < (5.0f - r) * 0.08f && t > 0.5f)
		{
			float3 v = normalize(-float3(cos(p), lerp(0.2f, 2.0f, t - 0.5f)));

			uint2 pixelCoord = (uint2)((uv - v.xy * .3) * FrameCB.renderResolution);
			result = outputTexture[pixelCoord].rgb;
		}
	}
	outputTexture[pixelCoord] += float4(result, 1.0f);
}