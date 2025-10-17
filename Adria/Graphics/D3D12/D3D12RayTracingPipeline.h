#pragma once
#include "Graphics/GfxRayTracingPipeline.h"


namespace adria
{
	class D3D12RayTracingPipeline : public GfxRayTracingPipeline
	{
	public:
		explicit D3D12RayTracingPipeline(ID3D12StateObject* state_object);
		virtual ~D3D12RayTracingPipeline() override;

		virtual Bool IsValid() const override;
		virtual void* GetNative() const override;
		virtual Bool HasShader(Char const* name) const override;

		ID3D12StateObject* GetD3D12StateObject() const;
		ID3D12StateObjectProperties* GetD3D12Properties() const;

	private:
		Ref<ID3D12StateObject> d3d12_state_object;
		Ref<ID3D12StateObjectProperties> d3d12_state_object_properties;
		std::unordered_set<std::string> shader_names;

	private:
		void CacheShaderNames();
	};
}