#pragma once
#include "Graphics/GfxBuffer.h"
#include "Utilities/Releasable.h"

namespace adria
{
	class D3D12Buffer : public GfxBuffer
	{
	public:
		D3D12Buffer(GfxDevice* gfx, GfxBufferDesc const& desc, GfxBufferData initial_data = {});
		~D3D12Buffer();

		virtual void* GetNative() const override;
		virtual Uint64 GetGpuAddress() const override;
		virtual void* GetSharedHandle() const override;
		ADRIA_MAYBE_UNUSED virtual void* Map() override;
		virtual void Unmap() override;
		virtual void SetName(Char const* name) override;

	private:
		Ref<ID3D12Resource> resource;
		ReleasablePtr<D3D12MA::Allocation> allocation = nullptr;
		HANDLE shared_handle = nullptr;
	};
}