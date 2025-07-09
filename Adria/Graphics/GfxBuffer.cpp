#include "GfxBuffer.h"
#include "GfxDevice.h"
#include "GfxCommandList.h"
#include "GfxLinearDynamicAllocator.h"

namespace adria
{

	GfxBuffer::GfxBuffer(GfxDevice* gfx, GfxBufferDesc const& desc, GfxBufferData initial_data) : gfx(gfx), desc(desc)
	{
		Uint64 buffer_size = desc.size;
		if (HasFlag(desc.misc_flags, GfxBufferMiscFlag::ConstantBuffer))
		{
			buffer_size = Align(buffer_size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
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
			cmd_list->CopyBuffer(
				*this,
				0,
				*upload_alloc.buffer,
				upload_alloc.offset,
				desc.size);

			if (HasFlag(desc.bind_flags, GfxBindFlag::ShaderResource))
			{
				cmd_list->BufferBarrier(*this, GfxResourceState::CopyDst, GfxResourceState::AllSRV);
				cmd_list->FlushBarriers();
			}
		}

	}

	GfxBuffer::~GfxBuffer()
	{
		if (mapped_data != nullptr)
		{
			ADRIA_ASSERT(resource != nullptr);
			resource->Unmap(0, nullptr);
			mapped_data = nullptr;
		}
	}

	void* GfxBuffer::GetMappedData() const
	{
		return mapped_data;
	}

	ID3D12Resource* GfxBuffer::GetNative() const
	{
		return resource.Get();
	}

	GfxBufferDesc const& GfxBuffer::GetDesc() const
	{
		return desc;
	}

	Uint64 GfxBuffer::GetGpuAddress() const
	{
		return resource->GetGPUVirtualAddress();
	}

	Uint64 GfxBuffer::GetSize() const
	{
		return desc.size;
	}

	Uint32 GfxBuffer::GetStride() const
	{
		return desc.stride;
	}

	Uint32 GfxBuffer::GetCount() const
	{
		ADRIA_ASSERT(desc.stride != 0);
		return static_cast<Uint32>(desc.size / desc.stride);
	}

	GfxFormat GfxBuffer::GetFormat() const
	{
		return desc.format;
	}

	void* GfxBuffer::GetSharedHandle() const
	{
		return shared_handle;
	}

	Bool GfxBuffer::IsMapped() const
	{
		return mapped_data != nullptr;
	}

	void* GfxBuffer::Map()
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

	void GfxBuffer::Unmap()
	{
		resource->Unmap(0, nullptr);
		mapped_data = nullptr;
	}

	void GfxBuffer::Update(void const* src_data, Uint64 data_size, Uint64 offset /*= 0*/)
	{
		ADRIA_ASSERT(desc.resource_usage == GfxResourceUsage::Upload);
		if (mapped_data)
		{
			memcpy((Uint8*)mapped_data + offset, src_data, data_size);
		}
		else
		{
			Map();
			ADRIA_ASSERT(mapped_data);
			memcpy((Uint8*)mapped_data + offset, src_data, data_size);
		}
	}

	void GfxBuffer::SetName(Char const* name)
	{
#if defined(_DEBUG) || defined(_PROFILE)
		resource->SetName(ToWideString(name).c_str());
#endif
	}
}