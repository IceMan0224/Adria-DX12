#include "D3D12RayTracingPipeline.h"
#include "D3D12Defines.h"
#include "Utilities/StringConversions.h"

namespace adria
{
	D3D12RayTracingPipeline::D3D12RayTracingPipeline(ID3D12StateObject* state_object)
		: d3d12_state_object(nullptr)
		, d3d12_state_object_properties(nullptr)
	{
		ADRIA_ASSERT(state_object != nullptr);
		d3d12_state_object.Attach(state_object);

		HRESULT hr = d3d12_state_object->QueryInterface(IID_PPV_ARGS(d3d12_state_object_properties.GetAddressOf()));
		D3D12_CHECK_CALL(hr);

		CacheShaderNames();
	}

	D3D12RayTracingPipeline::~D3D12RayTracingPipeline()
	{
		d3d12_state_object_properties.Reset();
		d3d12_state_object.Reset();
	}

	Bool D3D12RayTracingPipeline::IsValid() const
	{
		return d3d12_state_object != nullptr && d3d12_state_object_properties != nullptr;
	}

	void* D3D12RayTracingPipeline::GetNative() const
	{
		return d3d12_state_object.Get();
	}

	Bool D3D12RayTracingPipeline::HasShader(Char const* name) const
	{
		ADRIA_ASSERT(name != nullptr);
		return shader_names.find(name) != shader_names.end();
	}

	ID3D12StateObject* D3D12RayTracingPipeline::GetD3D12StateObject() const
	{
		return d3d12_state_object.Get();
	}

	ID3D12StateObjectProperties* D3D12RayTracingPipeline::GetD3D12Properties() const
	{
		return d3d12_state_object_properties.Get();
	}

	void D3D12RayTracingPipeline::CacheShaderNames()
	{
		shader_names.clear();
	}
}