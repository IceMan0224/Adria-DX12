#include "GfxStateObject.h"
#include "D3D12/D3D12Device.h"

namespace adria
{
	GfxStateObject* GfxStateObjectBuilder::CreateStateObject(GfxDevice* gfx, GfxStateObjectType type)
	{
		D3D12_STATE_OBJECT_TYPE d3d12_type;
		switch (type)
		{
		case GfxStateObjectType::RayTracingPipeline: d3d12_type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE; break;
		case GfxStateObjectType::Collection:		 d3d12_type = D3D12_STATE_OBJECT_TYPE_COLLECTION; break;
		case GfxStateObjectType::Executable:		 d3d12_type = D3D12_STATE_OBJECT_TYPE_EXECUTABLE; break;
		default: d3d12_type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
		}

		auto BuildDescription = [this](D3D12_STATE_OBJECT_TYPE type, D3D12_STATE_OBJECT_DESC& desc)
		{
			desc.Type = type;
			desc.NumSubobjects = static_cast<Uint32>(num_subobjects);
			desc.pSubobjects = num_subobjects ? subobjects.data() : nullptr;
		};
		D3D12_STATE_OBJECT_DESC desc{};
		BuildDescription(d3d12_type, desc);
		ID3D12StateObject* state_obj = nullptr;
		ID3D12Device5* d3d12gfx = (ID3D12Device5*)gfx->GetNativeDevice();
		HRESULT hr = d3d12gfx->CreateStateObject(&desc, IID_PPV_ARGS(&state_obj));
		GFX_CHECK_HR(hr);
		return new GfxStateObject(state_obj);
	}

}
