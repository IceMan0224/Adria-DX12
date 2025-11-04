#include "GfxBufferView.h"
#include "GfxBuffer.h"

namespace adria
{
	GfxVertexBufferView::GfxVertexBufferView(GfxBuffer* _buffer, Uint64 offset, Uint32 size, Uint32 stride)
		: buffer(_buffer)
		, buffer_location(_buffer->GetGpuAddress() + offset)
		, size_in_bytes(size == UINT32_MAX ? static_cast<Uint32>(_buffer->GetSize()) : size)
		, stride_in_bytes(stride == 0 ? _buffer->GetStride() : stride)
	{
	}

	GfxIndexBufferView::GfxIndexBufferView(GfxBuffer* _buffer, Uint64 offset, Uint32 size)
		: buffer(_buffer)
		, buffer_location(_buffer->GetGpuAddress() + offset)
		, size_in_bytes(size == UINT32_MAX ? static_cast<Uint32>(_buffer->GetSize()) : size)
		, format(_buffer->GetFormat())
	{
	}
}
