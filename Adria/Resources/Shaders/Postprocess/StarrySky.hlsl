#include "CommonResources.hlsli"

#define BLOCK_SIZE 16

struct StarrySkyConstants
{
	float  starsThreshold;
	float  starsExposure;
	float  starsIntensity;
	uint   depthIdx;
	uint   sceneIdx;
	uint   outputIdx;
};
DECLARE_CBUFFER(StarrySkyConstants, StarrySkyPassCB, 2);

float3 StarsHash(float3 p)
{
	p = float3(dot(p, float3(127.1, 311.7, 74.7)),
				dot(p, float3(269.5, 183.3, 246.1)),
				dot(p, float3(113.5, 271.9, 124.6)));
	return -1.0 + 2.0 * frac(sin(p) * 43758.5453123);
}

float StarsNoise(float3 p)
{
	float3 i = floor(p);
	float3 f = frac(p);
	float3 u = f * f * (3.0 - 2.0 * f);

	return lerp(lerp(lerp(dot(StarsHash(i + float3(0, 0, 0)), f - float3(0, 0, 0)),
						   dot(StarsHash(i + float3(1, 0, 0)), f - float3(1, 0, 0)), u.x),
					  lerp(dot(StarsHash(i + float3(0, 1, 0)), f - float3(0, 1, 0)),
						   dot(StarsHash(i + float3(1, 1, 0)), f - float3(1, 1, 0)), u.x), u.y),
				 lerp(lerp(dot(StarsHash(i + float3(0, 0, 1)), f - float3(0, 0, 1)),
						   dot(StarsHash(i + float3(1, 0, 1)), f - float3(1, 0, 1)), u.x),
					  lerp(dot(StarsHash(i + float3(0, 1, 1)), f - float3(0, 1, 1)),
						   dot(StarsHash(i + float3(1, 1, 1)), f - float3(1, 1, 1)), u.x), u.y), u.z);
}

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void StarrySkyCS(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	Texture2D<float> depthTexture = ResourceDescriptorHeap[StarrySkyPassCB.depthIdx];
	Texture2D sceneTexture = ResourceDescriptorHeap[StarrySkyPassCB.sceneIdx];
	RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[StarrySkyPassCB.outputIdx];

	float2 uv = (dispatchThreadId.xy + 0.5f) / FrameCB.renderResolution;
	float4 sceneColor = sceneTexture[dispatchThreadId.xy];
	float depth = depthTexture[dispatchThreadId.xy];

	if (depth == 0.0f)
	{
		float3 viewDir = normalize(GetWorldPosition(uv, 0.0001f) - FrameCB.cameraPosition);
		if (viewDir.y > 0.0f)
		{
			float stars = pow(clamp(StarsNoise(viewDir * 200.0f), 0.0f, 1.0f), StarrySkyPassCB.starsThreshold) * StarrySkyPassCB.starsExposure;
			stars *= lerp(0.4, 1.4, StarsNoise(viewDir * 100.0f + float3(FrameCB.totalTime, 0, 0)));
			sceneColor.rgb += stars * StarrySkyPassCB.starsIntensity;
		}
	}

	outputTexture[dispatchThreadId.xy] = sceneColor;
}
