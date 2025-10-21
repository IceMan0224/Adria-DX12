#include "GfxCommandList.h"
#include "GfxTexture.h"
#include "GfxBuffer.h"

namespace adria
{

	void GfxCommandList::ClearTexture(GfxTexture const& resource, Uint32 clear_value[4])
	{
		ClearTexture(resource, GfxTextureDescriptorDesc{}, clear_value);
	}

	void GfxCommandList::ClearTexture(GfxTexture const& resource, Float clear_value[4])
	{
		ClearTexture(resource, GfxTextureDescriptorDesc{}, clear_value);
	}

	void GfxCommandList::ClearBuffer(GfxBuffer const& resource, Uint32 clear_value[4])
	{
		ClearBuffer(resource, GfxBufferDescriptorDesc{}, clear_value);
	}

	void GfxCommandList::ClearBuffer(GfxBuffer const& resource, Float clear_value[4])
	{
		ClearBuffer(resource, GfxBufferDescriptorDesc{}, clear_value);
	}

}

