#include "D3D12Buffer.h"
#include "D3D12MemAlloc.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommandList.h"
#include "Graphics/GfxLinearDynamicAllocator.h"
#include "Utilities/Align.h"

namespace adria
{

	D3D12Buffer::D3D12Buffer(GfxDevice* gfx, GfxBufferDesc const& desc, GfxBufferData initial_data /*= {}*/) : GfxBuffer(gfx, desc)
	{
		Uint64 buffer_size = desc.size;
		if (HasFlag(desc.misc_flags, GfxBufferMiscFlag::ConstantBuffer))
		{
			buffer_size = AlignUp(buffer_size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		}

		D3D12_RESOURCE_DESC resource_desc{};
		resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resource_desc.Format = DXGI_FORMAT_UNKNOWN;
		resource_desc.Width = buffer_size;
		resource_desc.Height = 1;
		resource_desc.MipLevels = 1;
		resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resource_desc.DepthOrArraySize = 1;
		resource_desc.Alignment = 0;
		resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		resource_desc.SampleDesc.Count = 1;
		resource_desc.SampleDesc.Quality = 0;

		if (HasFlag(desc.bind_flags, GfxBindFlag::UnorderedAccess))
		{
			resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		}

		if (!HasFlag(desc.bind_flags, GfxBindFlag::ShaderResource))
		{
			resource_desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
		}

		D3D12_RESOURCE_STATES resource_state = D3D12_RESOURCE_STATE_COMMON;
		if (HasFlag(desc.misc_flags, GfxBufferMiscFlag::AccelStruct))
		{
			resource_state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		}

		D3D12MA::ALLOCATION_DESC allocation_desc{};
		allocation_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
		if (desc.resource_usage == GfxResourceUsage::Readback)
		{
			allocation_desc.HeapType = D3D12_HEAP_TYPE_READBACK;
			resource_state = D3D12_RESOURCE_STATE_COPY_DEST;
			resource_desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
		}
		else if (desc.resource_usage == GfxResourceUsage::Upload)
		{
			allocation_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
			resource_state = D3D12_RESOURCE_STATE_GENERIC_READ;
		}

		if (HasFlag(desc.misc_flags, GfxBufferMiscFlag::Shared))
		{
			allocation_desc.ExtraHeapFlags |= D3D12_HEAP_FLAG_SHARED;
		}

		ID3D12Device* device = gfx->GetDevice();
		D3D12MA::Allocator* allocator = gfx->GetAllocator();

		D3D12MA::Allocation* alloc = nullptr;
		HRESULT hr = allocator->CreateResource(
			&allocation_desc,
			&resource_desc,
			resource_state,
			nullptr,
			&alloc,
			IID_PPV_ARGS(resource.GetAddressOf())
		);
		GFX_CHECK_HR(hr);
		allocation.reset(alloc);

		if (HasFlag(desc.misc_flags, GfxBufferMiscFlag::Shared))
		{
			hr = gfx->GetDevice()->CreateSharedHandle(resource.Get(), nullptr, GENERIC_ALL, nullptr, &shared_handle);
			GFX_CHECK_HR(hr);
		}

		if (desc.resource_usage == GfxResourceUsage::Readback)
		{
			hr = resource->Map(0, nullptr, &mapped_data);
			GFX_CHECK_HR(hr);
		}
		else if (desc.resource_usage == GfxResourceUsage::Upload)
		{
			D3D12_RANGE read_range{};
			hr = resource->Map(0, &read_range, &mapped_data);
			GFX_CHECK_HR(hr);
			if (initial_data)
			{
				memcpy(mapped_data, initial_data, desc.size);
			}
		}

		if (initial_data != nullptr && desc.resource_usage != GfxResourceUsage::Upload)
		{
			GfxCommandList* cmd_list = gfx->GetGraphicsCommandList();
			GfxLinearDynamicAllocator* dynamic_allocator = gfx->GetDynamicAllocator();
			GfxDynamicAllocation upload_alloc = dynamic_allocator->Allocate(buffer_size);
			upload_alloc.Update(initial_data, desc.size);
			cmd_list->CopyBuffer(*this, 0, *upload_alloc.buffer, upload_alloc.offset, desc.size);

			if (HasFlag(desc.bind_flags, GfxBindFlag::ShaderResource))
			{
				cmd_list->BufferBarrier(*this, GfxResourceState::CopyDst, GfxResourceState::AllSRV);
				cmd_list->FlushBarriers();
			}
		}

	}

	D3D12Buffer::~D3D12Buffer()
	{
		if (mapped_data != nullptr)
		{
			ADRIA_ASSERT(resource != nullptr);
			resource->Unmap(0, nullptr);
			mapped_data = nullptr;
		}
	}

	void* D3D12Buffer::GetNative() const
	{
		return resource.Get();
	}

	Uint64 D3D12Buffer::GetGpuAddress() const
	{
		return resource->GetGPUVirtualAddress();
	}

	void* D3D12Buffer::GetSharedHandle() const
	{
		return shared_handle;
	}

	ADRIA_MAYBE_UNUSED void* D3D12Buffer::Map()
	{
		if (mapped_data)
		{
			return mapped_data;
		}

		HRESULT hr;
		if (desc.resource_usage == GfxResourceUsage::Readback)
		{
			hr = resource->Map(0, nullptr, &mapped_data);
			GFX_CHECK_HR(hr);
		}
		else if (desc.resource_usage == GfxResourceUsage::Upload)
		{
			D3D12_RANGE read_range{};
			hr = resource->Map(0, &read_range, &mapped_data);
			GFX_CHECK_HR(hr);
		}
		return mapped_data;
	}

	void D3D12Buffer::Unmap()
	{
		resource->Unmap(0, nullptr);
		mapped_data = nullptr;
	}

	void D3D12Buffer::SetName(Char const* name)
	{
#if defined(_DEBUG) || defined(_PROFILE)
		resource->SetName(ToWideString(name).c_str());
#endif
	}

}

