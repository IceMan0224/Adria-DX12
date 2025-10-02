#pragma once
#include "Graphics/GfxTexture.h"
#include "Utilities/Releasable.h"

namespace adria
{
	class D3D12Texture : public GfxTexture
	{
	public:
		D3D12Texture(GfxDevice* gfx, GfxTextureDesc const& desc, GfxTextureData const& data);
		D3D12Texture(GfxDevice* gfx, GfxTextureDesc const& desc);
		D3D12Texture(GfxDevice* gfx, GfxTextureDesc const& desc, void* backbuffer);
		~D3D12Texture();

		virtual void* GetNative() const override;
		virtual void* GetSharedHandle() const override;
		virtual Uint64 GetGpuAddress() const override;
		virtual void* Map() override;
		virtual void Unmap() override;
		virtual void SetName(Char const* name) override;
		virtual Uint32 GetRowPitch(Uint32 mip_level = 0) const override;

	private:
		Ref<ID3D12Resource> resource;
		ReleasablePtr<D3D12MA::Allocation> allocation = nullptr;
		HANDLE shared_handle = nullptr;
	};
}