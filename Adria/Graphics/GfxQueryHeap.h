#pragma once

namespace adria
{
	class GfxDevice;

	enum class GfxQueryType : Uint8
	{
		Occlusion,
		BinaryOcclusion,
		Timestamp,
		PipelineStatistics
	};
	struct GfxQueryHeapDesc
	{
		Uint32 count;
		GfxQueryType type;
	};

	class GfxQueryHeap
	{
	public:
		GfxQueryHeap(GfxDevice* gfx, GfxQueryHeapDesc const& desc) : gfx(gfx), desc(desc) {}
		virtual ~GfxQueryHeap() {}

		GfxDevice* GetParent() const { return gfx; }
		GfxQueryHeapDesc const& GetDesc() const { return desc; }

		virtual void* GetHandle() const = 0;

	private:
		GfxDevice* gfx;
		GfxQueryHeapDesc desc;
	};

}