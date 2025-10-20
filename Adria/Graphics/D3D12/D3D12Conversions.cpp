#include "D3D12Conversions.h"
#include "D3D12DescriptorHeap.h"

namespace adria
{
	D3D12_BARRIER_SYNC ToD3D12BarrierSync(GfxResourceState flags)
	{
		using enum GfxResourceState;

		D3D12_BARRIER_SYNC sync = D3D12_BARRIER_SYNC_NONE;
		Bool const discard = HasFlag(flags, Discard);
		if (!discard && HasFlag(flags, ClearUAV))
		{
			sync |= D3D12_BARRIER_SYNC_CLEAR_UNORDERED_ACCESS_VIEW;
		}

		if (HasFlag(flags, Present))		sync |= D3D12_BARRIER_SYNC_ALL;
		if (HasFlag(flags, Common))			sync |= D3D12_BARRIER_SYNC_ALL;
		if (HasFlag(flags, RTV))			sync |= D3D12_BARRIER_SYNC_RENDER_TARGET;
		if (HasAnyFlag(flags, AllDSV))		sync |= D3D12_BARRIER_SYNC_DEPTH_STENCIL;
		if (HasAnyFlag(flags, AllVertex))	sync |= D3D12_BARRIER_SYNC_VERTEX_SHADING;
		if (HasAnyFlag(flags, AllPixel))	sync |= D3D12_BARRIER_SYNC_PIXEL_SHADING;
		if (HasAnyFlag(flags, AllCompute))	sync |= D3D12_BARRIER_SYNC_COMPUTE_SHADING;
		if (HasAnyFlag(flags, AllCopy))		sync |= D3D12_BARRIER_SYNC_COPY;
		if (HasFlag(flags, ShadingRate))	sync |= D3D12_BARRIER_SYNC_PIXEL_SHADING;
		if (HasFlag(flags, IndexBuffer))	sync |= D3D12_BARRIER_SYNC_INDEX_INPUT;
		if (HasFlag(flags, IndirectArgs))	sync |= D3D12_BARRIER_SYNC_EXECUTE_INDIRECT;
		if (HasAnyFlag(flags, AllAS))		sync |= D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE;
		return sync;
	}

	D3D12_BARRIER_LAYOUT ToD3D12BarrierLayout(GfxResourceState flags)
	{
		using enum GfxResourceState;

		if (HasFlag(flags, CopySrc) && HasAnyFlag(flags, AllSRV) && !HasFlag(flags, DSV_ReadOnly)) return D3D12_BARRIER_LAYOUT_GENERIC_READ;
		if (HasFlag(flags, CopyDst) && HasAnyFlag(flags, AllUAV)) return D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COMMON;
		if (HasFlag(flags, DSV_ReadOnly) && (HasAnyFlag(flags, AllSRV) || HasFlag(flags, CopySrc))) return D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_GENERIC_READ;
		if (HasFlag(flags, Discard))		return D3D12_BARRIER_LAYOUT_UNDEFINED;
		if (HasFlag(flags, Present))		return D3D12_BARRIER_LAYOUT_PRESENT;
		if (HasFlag(flags, RTV))			return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
		if (HasFlag(flags, DSV))            return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
		if (HasFlag(flags, DSV_ReadOnly))   return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
		if (HasAnyFlag(flags, AllSRV))		return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
		if (HasAnyFlag(flags, AllUAV))		return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
		if (HasFlag(flags, ClearUAV))		return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
		if (HasFlag(flags, CopyDst))		return D3D12_BARRIER_LAYOUT_COPY_DEST;
		if (HasFlag(flags, CopySrc))		return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
		if (HasFlag(flags, ShadingRate))	return D3D12_BARRIER_LAYOUT_SHADING_RATE_SOURCE;
		ADRIA_UNREACHABLE();
		return D3D12_BARRIER_LAYOUT_UNDEFINED;
	}

	D3D12_BARRIER_ACCESS ToD3D12BarrierAccess(GfxResourceState flags)
	{
		using enum GfxResourceState;
		if (HasFlag(flags, Discard))
		{
			return D3D12_BARRIER_ACCESS_NO_ACCESS;
		}

		D3D12_BARRIER_ACCESS access = D3D12_BARRIER_ACCESS_COMMON;
		if (HasFlag(flags, RTV))             access |= D3D12_BARRIER_ACCESS_RENDER_TARGET;
		if (HasFlag(flags, DSV))             access |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
		if (HasFlag(flags, DSV_ReadOnly))    access |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
		if (HasAnyFlag(flags, AllSRV))       access |= D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
		if (HasAnyFlag(flags, AllUAV))       access |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
		if (HasFlag(flags, ClearUAV))        access |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
		if (HasFlag(flags, CopyDst))         access |= D3D12_BARRIER_ACCESS_COPY_DEST;
		if (HasFlag(flags, CopySrc))         access |= D3D12_BARRIER_ACCESS_COPY_SOURCE;
		if (HasFlag(flags, ShadingRate))     access |= D3D12_BARRIER_ACCESS_SHADING_RATE_SOURCE;
		if (HasFlag(flags, IndexBuffer))     access |= D3D12_BARRIER_ACCESS_INDEX_BUFFER;
		if (HasFlag(flags, IndirectArgs))    access |= D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT;
		if (HasFlag(flags, ASRead))          access |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ;
		if (HasFlag(flags, ASWrite))         access |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE;
		return access;
	}

	constexpr D3D12_RESOURCE_STATES ToD3D12LegacyResourceState(GfxResourceState state)
	{
		using enum GfxResourceState;

		D3D12_RESOURCE_STATES api_state = D3D12_RESOURCE_STATE_COMMON;
		if (HasAnyFlag(state, IndexBuffer))		api_state |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
		if (HasFlag(state, RTV))				api_state |= D3D12_RESOURCE_STATE_RENDER_TARGET;
		if (HasAnyFlag(state, AllUAV))			api_state |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		if (HasFlag(state, DSV))				api_state |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
		if (HasFlag(state, DSV_ReadOnly))		api_state |= D3D12_RESOURCE_STATE_DEPTH_READ;
		if (HasAnyFlag(state, ComputeSRV | VertexSRV)) api_state |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		if (HasFlag(state, PixelSRV))			api_state |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		if (HasFlag(state, IndirectArgs))		api_state |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
		if (HasFlag(state, CopyDst))			api_state |= D3D12_RESOURCE_STATE_COPY_DEST;
		if (HasFlag(state, CopySrc))			api_state |= D3D12_RESOURCE_STATE_COPY_SOURCE;
		if (HasFlag(state, Present))			api_state |= D3D12_RESOURCE_STATE_PRESENT;
		if (HasAnyFlag(state, AllAS))			api_state |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		return api_state;
	}

}