#pragma once
#include "Graphics/GfxQueryHeap.h"

@protocol MTLCounterSampleBuffer;

namespace adria
{
    class MetalQueryHeap final : public GfxQueryHeap
    {
    public:
        MetalQueryHeap(GfxDevice* gfx, GfxQueryHeapDesc const& desc);
        virtual ~MetalQueryHeap() override;

        virtual void* GetHandle() const override;

        id<MTLCounterSampleBuffer> GetCounterSampleBuffer() const { return counter_sample_buffer; }
        Bool IsValid() const { return counter_sample_buffer != nil; }

    private:
        id<MTLCounterSampleBuffer> counter_sample_buffer = nil;
    };
}
