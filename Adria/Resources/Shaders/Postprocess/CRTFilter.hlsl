#include "CommonResources.hlsli"

//credits: https://www.shadertoy.com/view/XsjSzR

#define BLOCK_SIZE 16

struct CRTFilterConstants
{
	uint   inputIdx;
	uint   outputIdx;
	float  hardScan;
	float  pixelHardness;
	float  warpX;
	float  warpY;
};
ConstantBuffer<CRTFilterConstants> CRTFilterPassCB : register(b1);

static const float MASK_DARK  = 0.5f;
static const float MASK_LIGHT = 1.5f;

float ToLinear1(float c)
{
	return (c <= 0.04045) ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

float3 ToLinear(float3 c)
{
	return float3(ToLinear1(c.r), ToLinear1(c.g), ToLinear1(c.b));
}


float ToSrgb1(float c)
{
	return (c < 0.0031308 ? c * 12.92 : 1.055 * pow(c, 0.41666) - 0.055);
}

float3 ToSrgb(float3 c)
{
	return float3(ToSrgb1(c.r), ToSrgb1(c.g), ToSrgb1(c.b));
}

float3 Fetch(float2 pos,float2 off)
{
	float2 res = FrameCB.displayResolution.xy/6.0;
	Texture2D<float4> inputTexture = ResourceDescriptorHeap[CRTFilterPassCB.inputIdx];
	pos = floor(pos * res + off) / res;
	if (max(abs(pos.x - 0.5), abs(pos.y - 0.5)) > 0.5)
	{
		return 0.0f;
	}
	return ToLinear(inputTexture.SampleLevel(LinearClampSampler, pos.xy, 0).rgb);
}

float2 Dist(float2 pos)
{
	float2 res = FrameCB.displayResolution.xy/6.0;
	pos=pos * res;
	return -((pos - floor(pos)) - 0.5);
}

float Gaus(float pos, float scale)
{
	return exp2(scale * pos * pos);
}

float3 Horz3(float2 pos,float off)
{
	float3 b = Fetch(pos, float2(-1.0, off));
	float3 c = Fetch(pos, float2( 0.0, off));
	float3 d = Fetch(pos, float2( 1.0, off));
	float dst = Dist(pos).x;

	float scale = CRTFilterPassCB.pixelHardness;
	float wb = Gaus(dst - 1.0, scale);
	float wc = Gaus(dst + 0.0, scale);
	float wd = Gaus(dst + 1.0, scale);
	return (b * wb + c * wc + d * wd)/(wb + wc + wd);
}

float3 Horz5(float2 pos, float off)
{
	float3 a = Fetch(pos, float2(-2.0, off));
	float3 b = Fetch(pos, float2(-1.0, off));
	float3 c = Fetch(pos, float2( 0.0, off));
	float3 d = Fetch(pos, float2( 1.0, off));
	float3 e = Fetch(pos, float2( 2.0, off));

	float dst  = Dist(pos).x;
	float scale = CRTFilterPassCB.pixelHardness;
	float wa = Gaus(dst-2.0,scale);
	float wb = Gaus(dst-1.0,scale);
	float wc = Gaus(dst+0.0,scale);
	float wd = Gaus(dst+1.0,scale);
	float we = Gaus(dst+2.0,scale);
	return (a * wa + b * wb + c * wc + d * wd + e * we)/(wa + wb + wc + wd + we);
}

float Scan(float2 pos, float off)
{
	float dst = Dist(pos).y;
	return Gaus(dst + off, CRTFilterPassCB.hardScan);
}

float3 Tri(float2 pos)
{
	float3 a = Horz3(pos,-1.0);
	float3 b = Horz5(pos, 0.0);
	float3 c = Horz3(pos, 1.0);
	float wa = Scan(pos,-1.0);
	float wb = Scan(pos, 0.0);
	float wc = Scan(pos, 1.0);
	return a * wa + b * wb + c * wc;
}

float2 Warp(float2 pos)
{
	pos = pos * 2.0f - 1.0f;
	pos *= float2(1.0f + (pos.y * pos.y) * CRTFilterPassCB.warpX, 1.0f + (pos.x * pos.x) * CRTFilterPassCB.warpY);
	return pos * 0.5f + 0.5f;
}

float3 Mask(float2 pos)
{
	pos.x += pos.y * 3.0;
	float3 mask = float3(MASK_DARK, MASK_DARK, MASK_DARK);
	pos.x = frac(pos.x / 6.0);
	if(pos.x<0.333)
	{
		mask.r = MASK_LIGHT;
	}
	else if (pos.x < 0.666)
	{
		mask.g = MASK_LIGHT;
	}
	else
	{
		mask.b = MASK_LIGHT;
	}
	return mask;
}

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void CRTFilterCS(CSInput input)
{
	RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[CRTFilterPassCB.outputIdx];

	float4 color = 0.0f;
	float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.displayResolution);
	uv = Warp(uv);
	color.rgb=Tri(uv)*Mask(input.DispatchThreadId.xy);
	color.rgb=ToSrgb(color.rgb);
	outputTexture[input.DispatchThreadId.xy] = color;
}