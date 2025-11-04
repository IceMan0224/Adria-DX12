#pragma once
#include "Graphics/GfxBuffer.h"

@protocol MTLBuffer;

namespace adria
{
    class MetalBuffer final : public GfxBuffer
    {
    public:
        MetalBuffer(GfxDevice* gfx, GfxBufferDesc const& desc, GfxBufferData initial_data = {});
        virtual ~MetalBuffer() override;

        virtual void* GetNative() const override;
        virtual Uint64 GetGpuAddress() const override;
        virtual void* GetSharedHandle() const override;
        virtual void* Map() override;
        virtual void Unmap() override;
        virtual void SetName(Char const* name) override;

        id<MTLBuffer> GetMetalBuffer() const { return metal_buffer; }

    private:
        id<MTLBuffer> metal_buffer;
    };
}
