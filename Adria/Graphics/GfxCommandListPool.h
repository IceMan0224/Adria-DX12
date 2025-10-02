#pragma once

namespace adria
{
	class GfxDevice;
	class GfxCommandList;
	enum class GfxCommandListType : Uint8;

	class GfxCommandListPool
	{
		friend class GfxCommandQueue;

	public:
		ADRIA_NONCOPYABLE(GfxCommandListPool)
		ADRIA_DEFAULT_MOVABLE(GfxCommandListPool)
		~GfxCommandListPool() = default;

		GfxCommandList* GetMainCmdList() const;
		GfxCommandList* GetLatestCmdList() const;

		GfxCommandList* AllocateCmdList();
		void FreeCmdList(GfxCommandList* _cmd_list);

		void BeginCmdLists();
		void EndCmdLists();

		auto begin() { return cmd_lists.begin(); }
		auto end() { return   cmd_lists.end(); }

	protected:
		GfxCommandListPool(GfxDevice* gfx, GfxCommandListType type);

	private:
		GfxDevice* gfx;
		GfxCommandListType const type;
		std::vector<std::unique_ptr<GfxCommandList>> cmd_lists;
	};

	class GfxGraphicsCommandListPool : public GfxCommandListPool
	{
	public:
		explicit GfxGraphicsCommandListPool(GfxDevice* gfx);
	};

	class GfxComputeCommandListPool : public GfxCommandListPool
	{
	public:
		explicit GfxComputeCommandListPool(GfxDevice* gfx);
	};

	class GfxCopyCommandListPool : public GfxCommandListPool
	{
	public:
		explicit GfxCopyCommandListPool(GfxDevice* gfx);
	};

}