#include "DDGICommon.hlsli"
#include "Lighting.hlsli"
#include "Common.hlsli"
#include "RayTracing/RayTracingUtil.hlsli"

struct DDGIRayTracePassConstants
{
	float3 randomVector;
	float  randomAngle;
	float  historyBlendWeight;
	uint   rayBufferIdx;
};
ConstantBuffer<DDGIRayTracePassConstants> DDGIRayTracePassCB : register(b1);

[numthreads(32, 1, 1)]
void DDGIRayTraceCS(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	StructuredBuffer<DDGIVolume> ddgiVolumeBuffer = ResourceDescriptorHeap[FrameCB.ddgiVolumesIdx];
	DDGIVolume ddgiVolume = ddgiVolumeBuffer[0];

	uint const rayIdx   = dispatchThreadId.x;
	uint const probeIdx = dispatchThreadId.y;

	if (rayIdx >= (uint)ddgiVolume.raysPerProbe)
		return;

	float3x3 randomRotation  = AngleAxis3x3(DDGIRayTracePassCB.randomAngle, DDGIRayTracePassCB.randomVector);
	float3   randomDirection = normalize(mul(SphericalFibonacci(rayIdx, ddgiVolume.raysPerProbe), randomRotation));

	float3 probeLocation = GetProbeLocation(ddgiVolume, probeIdx);

	RayDesc ray;
	ray.Origin    = probeLocation;
	ray.Direction = randomDirection;
	ray.TMin      = 0.01f;
	ray.TMax      = FLT_MAX;

	float3 radiance = 0.0f;
	float  hitDistance = Max(ddgiVolume.probeSize) * 2;

	HitInfo hitInfo;
	if (TraceRay(ray, hitInfo))
	{
		Instance instanceData = GetInstanceData(hitInfo.instanceIndex);
		Mesh     meshData     = GetMeshData(instanceData.meshIndex);
		Material materialData = GetMaterialData(instanceData.materialIdx);
		VertexData vertex     = LoadVertexData(meshData, hitInfo.primitiveIndex, hitInfo.barycentricCoordinates);

		float4 posWS       = mul(float4(vertex.pos, 1.0f), instanceData.worldMatrix);
		float3 worldPosition = posWS.xyz / posWS.w;
		float3 worldNormal = mul(vertex.nor, (float3x3) transpose(instanceData.inverseWorldMatrix));
		float3 V           = normalize(FrameCB.cameraPosition - worldPosition);

		MaterialProperties matProperties = GetMaterialProperties(materialData, vertex.uv, 6);
		BrdfData brdfData = GetBrdfData(matProperties);

		float3 N = normalize(worldNormal);
		for (uint lightIndex = 0; lightIndex < FrameCB.lightCount; ++lightIndex)
		{
			LightInfo lightInfo = LoadLightInfo(lightIndex);

			bool visibility = TraceShadowRay(lightInfo, worldPosition, FrameCB.inverseView);
			if (!visibility) continue;

			float3 L = mul(lightInfo.direction.xyz, (float3x3) FrameCB.inverseView);
			L = normalize(-L);
			float3 diffuse = saturate(dot(L, N)) * DiffuseBRDF(brdfData.Diffuse);
			radiance += diffuse * lightInfo.color.rgb;
		}

		radiance += matProperties.emissive;
		radiance += DiffuseBRDF(min(brdfData.Diffuse, 0.9f)) * SampleDDGIIrradiance(ddgiVolume, worldPosition, N, ray.Direction);
		hitDistance = min(hitInfo.hitT, hitDistance);
	}
	else
	{
		TextureCube envMapTexture = ResourceDescriptorHeap[FrameCB.envMapIdx];
		radiance = envMapTexture.SampleLevel(LinearWrapSampler, randomDirection, 0).rgb;
	}

	RWBuffer<float4> rayBuffer = ResourceDescriptorHeap[DDGIRayTracePassCB.rayBufferIdx];
	rayBuffer[probeIdx * ddgiVolume.maxRaysPerProbe + rayIdx] = float4(radiance, hitDistance);
}
