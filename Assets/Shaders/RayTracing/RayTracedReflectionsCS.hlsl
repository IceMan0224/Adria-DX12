#include "Constants.hlsli"
#include "Scene.hlsli"
#include "Lighting.hlsli"
#include "RayTracingUtil.hlsli"
#include "Packing.hlsli"

struct RayTracedReflectionsConstants
{
    float roughnessScale;
    uint  depthIdx;
	uint  normalIdx;
	uint  albedoIdx;
	uint  outputIdx;
};
DECLARE_CBUFFER(RayTracedReflectionsConstants, RayTracedReflectionsPassCB, 1);

[numthreads(16, 16, 1)]
void RayTracedReflectionsCS(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	uint2 launchIndex = dispatchThreadId.xy;
	if (launchIndex.x >= (uint)FrameCB.renderResolution.x || launchIndex.y >= (uint)FrameCB.renderResolution.y)
		return;

	RWTexture2D<float4> outputTexture          = ResourceDescriptorHeap[RayTracedReflectionsPassCB.outputIdx];
	Texture2D<float>    depthTexture           = ResourceDescriptorHeap[RayTracedReflectionsPassCB.depthIdx];
	Texture2D           normalMetallicTexture  = ResourceDescriptorHeap[RayTracedReflectionsPassCB.normalIdx];
	Texture2D           albedoRoughnessTexture = ResourceDescriptorHeap[RayTracedReflectionsPassCB.albedoIdx];

	float depth = depthTexture.Load(int3(launchIndex, 0));
	float2 uv = (launchIndex + 0.5f) / FrameCB.renderResolution;

	float4 normalMetallic = normalMetallicTexture.Load(int3(launchIndex, 0));
	float3 viewNormal;
	float metallic;
	uint shadingExtension;
	DecodeGBufferNormalRT(normalMetallic, viewNormal, metallic, shadingExtension);

	float roughness    = albedoRoughnessTexture.Load(int3(launchIndex, 0)).a;
	float reflectivity = saturate((1.0f - roughness) * (1.0f - roughness));

	if (depth > 0.00001f && reflectivity > 0.0f)
	{
		float3 worldNormal   = normalize(mul(viewNormal, (float3x3) transpose(FrameCB.view)));
		float3 worldPosition = GetWorldPosition(uv, depth);

		float3 V      = normalize(worldPosition - FrameCB.cameraPosition);
		float3 rayDir = reflect(V, worldNormal);

		uint2 launchDim = (uint2)FrameCB.renderResolution;
		RNG rng = RNG_Initialize(launchIndex.x + launchIndex.y * launchDim.x, 0, 16);
		rayDir = GetConeSample(rng, rayDir, RayTracedReflectionsPassCB.roughnessScale);

		RayDesc ray;
		ray.Origin    = worldPosition;
		ray.Direction = rayDir;
		ray.TMin      = 1e-2f;
		ray.TMax      = FLT_MAX;

		HitInfo hitInfo;
		float3 reflectionColor = 0.0f;
		if (TraceRay(ray, hitInfo))
		{
			Instance instanceData = GetInstanceData(hitInfo.instanceIndex);
			Mesh     meshData     = GetMeshData(instanceData.meshIndex);
			Material materialData = GetMaterialData(instanceData.materialIdx);

			VertexData vertex = LoadVertexData(meshData, hitInfo.primitiveIndex, hitInfo.barycentricCoordinates);

			float4 posWS    = mul(float4(vertex.pos, 1.0f), instanceData.worldMatrix);
			float3 hitPos   = posWS.xyz / posWS.w;
			float3 hitNorWS = normalize(mul(vertex.nor, (float3x3) transpose(instanceData.inverseWorldMatrix)));
			float3 Vhit     = normalize(FrameCB.cameraPosition - hitPos);

			MaterialProperties matProps = GetMaterialProperties(materialData, vertex.uv, 2);

			float3 radiance = 0.0f;
			for (int i = 0; i < FrameCB.lightCount; ++i)
			{
				LightInfo lightInfo = LoadLightInfo(i);
				bool visibility = TraceShadowRay(lightInfo, hitPos, FrameCB.inverseView);
				if (!visibility) continue;

				radiance += DoLightNoShadows_Default(lightInfo, hitPos, hitNorWS, Vhit,
				                                     matProps.baseColor, matProps.metallic, matProps.roughness);
			}
			radiance += matProps.emissive;
			reflectionColor = radiance;
		}
		else
		{
			TextureCube envMap = ResourceDescriptorHeap[FrameCB.envMapIdx];
			reflectionColor = envMap.SampleLevel(LinearWrapSampler, rayDir, 0).rgb;
		}

		outputTexture[launchIndex] = float4(reflectivity * reflectionColor, 1.0f);
	}
	else
	{
		outputTexture[launchIndex] = 0.0f;
	}
}
