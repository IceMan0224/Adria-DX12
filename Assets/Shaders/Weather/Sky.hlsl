#include "CommonResources.hlsli"
#include "Atmosphere.hlsli"

struct VertexIn
{
	float3 PosL : POSITION;
};
struct VSToPS
{
	float4 Pos : SV_POSITION;
	float3 PosL : POSITION;
};

VSToPS SkyVS(VertexIn input)
{
	VSToPS output = (VSToPS)0;
	output.Pos = float4(input.PosL + FrameCB.cameraPosition, 1.0f);
	output.Pos = mul(output.Pos, FrameCB.viewProjection).xyww;
	output.Pos.z = 0.0f;
	output.PosL = input.PosL;
	return output;
}
float4 SkyPS(VSToPS input) : SV_Target
{
	TextureCube skyCubemapTexture = ResourceDescriptorHeap[FrameCB.envMapIdx];
	return skyCubemapTexture.Sample(LinearWrapSampler, input.PosL);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

#define BLOCK_SIZE 16

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint GroupIndex : SV_GroupIndex;
};

struct EnvMapConstants
{
	uint envMapIdx;
};
DECLARE_CBUFFER(EnvMapConstants, EnvMapPassCB, 1);

struct HosekWilkieConstants
{
	float4 A;
	float4 B;
	float4 C;
	float4 D;
	float4 E;
	float4 F;
	float4 G;
	float4 H;
	float4 I;
	float4 Z;
};
DECLARE_CBUFFER(HosekWilkieConstants, HosekWilkiePassCB, 3);
float3 HosekWilkie(float cosTheta, float gamma, float cosGamma)
{
	const float3 A = HosekWilkiePassCB.A.xyz;
	const float3 B = HosekWilkiePassCB.B.xyz;
	const float3 C = HosekWilkiePassCB.C.xyz;
	const float3 D = HosekWilkiePassCB.D.xyz;
	const float3 E = HosekWilkiePassCB.E.xyz;
	const float3 F = HosekWilkiePassCB.F.xyz;
	const float3 G = HosekWilkiePassCB.G.xyz;
	const float3 H = HosekWilkiePassCB.H.xyz;
	const float3 I = HosekWilkiePassCB.I.xyz;

	float3 chi = (1 + cosGamma * cosGamma) / pow(1 + H * H - 2 * cosGamma * H, float3(1.5f, 1.5f, 1.5f));
	return (1 + A * exp(B / (cosTheta + 0.01))) * (C + D * exp(E * gamma) + F * (cosGamma * cosGamma) + G * chi + I * sqrt(cosTheta));
}
float3 HosekWilkieSky(float3 v, float3 sunDir)
{
	float cosTheta = clamp(v.y, 0, 1);
	float cosGamma = clamp(dot(v, sunDir), 0, 1);
	float gamma = acos(cosGamma);
	float3 R = -HosekWilkiePassCB.Z.xyz * HosekWilkie(cosTheta, gamma, cosGamma);
	if (any(isnan(R)) || any(isinf(R))) R = 0;
	return R;
}

float3 GetRayDir(uint3 threadId)
{
	const float2 outputSize = float2(128, 128);
	float2 st = threadId.xy / outputSize;
	float2 uv = 2.0 * float2(st.x, 1.0 - st.y) - float2(1.0, 1.0);

	// Select vector based on cubemap face index.
	float3 ret;
	switch (threadId.z)
	{
	case 0: ret = float3(1.0, uv.y, -uv.x); break;
	case 1: ret = float3(-1.0, uv.y, uv.x); break;
	case 2: ret = float3(uv.x, 1.0, -uv.y); break;
	case 3: ret = float3(uv.x, -1.0, uv.y); break;
	case 4: ret = float3(uv.x, uv.y, 1.0); break;
	case 5: ret = float3(-uv.x, uv.y, -1.0); break;
	}
	return normalize(ret);
}

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void HosekWilkieSkyCS(CSInput input)
{
	RWTexture2DArray<float4> envMapTx = ResourceDescriptorHeap[EnvMapPassCB.envMapIdx];
	uint3 threadId = input.DispatchThreadId;

	float3 rayStart = FrameCB.cameraPosition;
	float3 rayDir = GetRayDir(threadId);
	float rayLength = INFINITY;

	float3 color = HosekWilkieSky(rayDir, normalize(FrameCB.sunDirection.xyz));
	envMapTx[threadId] = float4(color, 1.0f);
}

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void MinimalAtmosphereSkyCS(CSInput input)
{
	RWTexture2DArray<float4> envMapTx = ResourceDescriptorHeap[EnvMapPassCB.envMapIdx];
	uint3 threadId = input.DispatchThreadId;

	float3 rayStart = FrameCB.cameraPosition;
	float3 rayDir = GetRayDir(threadId);
	float rayLength = INFINITY;

	bool PlanetShadow = false;
	if (PlanetShadow)
	{
		float2 planetIntersection = PlanetIntersection(rayStart, rayDir);
		if (planetIntersection.x > 0)
		{
			rayLength = min(rayLength, planetIntersection.x);
		}
	}

	float3 lightDir = normalize(FrameCB.sunDirection.xyz);
	float3 lightColor = FrameCB.sunColor.xyz;

	float3 transmittance;
	float3 color = IntegrateScattering(rayStart, rayDir, rayLength, lightDir, lightColor, 16, transmittance);
	envMapTx[threadId] = float4(color, 1.0f);
}