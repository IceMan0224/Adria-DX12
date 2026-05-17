#pragma once
#include "GfxBuffer.h"
#include "GfxDefines.h"
#include "GfxFormat.h"

namespace adria
{
	struct GfxVertexBufferView
	{
		explicit GfxVertexBufferView(GfxBuffer* buffer)
			: buffer_location(buffer->GetGpuAddress()), size_in_bytes((Uint32)buffer->GetSize()), stride_in_bytes(buffer->GetStride())
		{
		}

		GfxVertexBufferView(Uint64 buffer_location, Uint32 count, Uint32 stride_in_bytes)
			: buffer_location(buffer_location), size_in_bytes(count * stride_in_bytes), stride_in_bytes(stride_in_bytes)
		{
		}

		Uint64                      buffer_location = 0;
		Uint32                      size_in_bytes = 0;
		Uint32                      stride_in_bytes = 0;
	};

	struct GfxIndexBufferView
	{
		explicit GfxIndexBufferView(GfxBuffer* buffer)
			: buffer_location(buffer->GetGpuAddress()), size_in_bytes((Uint32)buffer->GetSize()), format(buffer->GetFormat())
		{
		}

		GfxIndexBufferView(Uint64 buffer_location, Uint32 count, GfxFormat format = GfxFormat::R32_UINT)
			: buffer_location(buffer_location), size_in_bytes(count * GetGfxFormatStride(format)), format(format)
		{
		}

		Uint64                      buffer_location = 0;
		Uint32                      size_in_bytes;
		GfxFormat                   format;
	};

	inline GfxBufferDesc VertexBufferDesc(Uint64 vertex_count, Uint32 stride, Bool ray_tracing = true)
	{
		GfxBufferDesc desc{};
		desc.bind_flags = ray_tracing ? GfxBindFlag::ShaderResource : GfxBindFlag::None;
		desc.resource_usage = GfxResourceUsage::Default;
		desc.size = vertex_count * stride;
		desc.stride = stride;
		return desc;
	}
	inline GfxBufferDesc IndexBufferDesc(Uint64 index_count, Bool small_indices, Bool ray_tracing = true)
	{
		GfxBufferDesc desc{};
		desc.bind_flags = ray_tracing ? GfxBindFlag::ShaderResource : GfxBindFlag::None;
		desc.resource_usage = GfxResourceUsage::Default;
		desc.stride = small_indices ? 2 : 4;
		desc.size = index_count * desc.stride;
		desc.format = small_indices ? GfxFormat::R16_UINT : GfxFormat::R32_UINT;
		return desc;
	}
	inline GfxBufferDesc ReadBackBufferDesc(Uint64 size)
	{
		GfxBufferDesc desc{};
		desc.bind_flags = GfxBindFlag::None;
		desc.resource_usage = GfxResourceUsage::Readback;
		desc.size = size;
		desc.misc_flags = GfxBufferMiscFlag::None;
		return desc;
	}
	template<typename T>
	inline GfxBufferDesc StructuredBufferDesc(Uint64 count, Bool uav = true, Bool dynamic = false)
	{
		ADRIA_ASSERT_MSG(uav ^ dynamic, "Buffer cannot be dynamic and be accessed as UAV at the same time!");
		GfxBufferDesc desc{};
		desc.resource_usage = (uav || !dynamic) ? GfxResourceUsage::Default : GfxResourceUsage::Upload;
		desc.bind_flags = GfxBindFlag::ShaderResource;
		if (uav)
		{
			desc.bind_flags |= GfxBindFlag::UnorderedAccess;
		}
		desc.misc_flags = GfxBufferMiscFlag::BufferStructured;
		desc.stride = sizeof(T);
		desc.size = desc.stride * count;
		return desc;
	}
	inline GfxBufferDesc CounterBufferDesc()
	{
		GfxBufferDesc desc{};
		desc.size = sizeof(Uint32);
		desc.bind_flags = GfxBindFlag::UnorderedAccess;
		return desc;
	}
}
