#include "D3D12CommandQueue.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommandList.h"
#include "Graphics/GfxCommandListPool.h"
#include "Utilities/StringConversions.h"

namespace adria
{
	D3D12CommandQueue::D3D12CommandQueue(GfxDevice* gfx, GfxCommandListType type, Char const* name)
		: type(type)
	{
		ID3D12Device* device = gfx->GetDevice(); // Assuming GfxDevice exposes the native D3D12 device

		auto ToD3D12CmdListType = [](GfxCommandListType type)
			{
				switch (type)
				{
				case GfxCommandListType::Graphics:
					return D3D12_COMMAND_LIST_TYPE_DIRECT;
				case GfxCommandListType::Compute:
					return D3D12_COMMAND_LIST_TYPE_COMPUTE;
				case GfxCommandListType::Copy:
					return D3D12_COMMAND_LIST_TYPE_COPY;
				}
				return D3D12_COMMAND_LIST_TYPE_NONE;
			};

		D3D12_COMMAND_QUEUE_DESC queue_desc{};
		queue_desc.Type = ToD3D12CmdListType(type);
		queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queue_desc.NodeMask = 0;

		HRESULT hr = device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(command_queue.GetAddressOf()));
		GFX_CHECK_HR(hr);

		command_queue->SetName(ToWideString(name).c_str());
		if (type != GfxCommandListType::Copy)
		{
			command_queue->GetTimestampFrequency(&timestamp_frequency);
		}
	}

	void D3D12CommandQueue::ExecuteCommandLists(std::span<GfxCommandList*> cmd_lists)
	{
		if (cmd_lists.empty())
		{
			return;
		}

		for (GfxCommandList* cmd_list : cmd_lists)
		{
			cmd_list->WaitAll();
		}

		std::vector<ID3D12CommandList*> d3d12_cmd_lists(cmd_lists.size());
		for (Uint64 i = 0; i < d3d12_cmd_lists.size(); ++i)
		{
			d3d12_cmd_lists[i] = cmd_lists[i]->GetNative();
		}
		command_queue->ExecuteCommandLists((Uint32)d3d12_cmd_lists.size(), d3d12_cmd_lists.data());

		for (GfxCommandList* cmd_list : cmd_lists)
		{
			cmd_list->SignalAll();
		}
	}

	void D3D12CommandQueue::Signal(GfxFence& fence, uint64_t fence_value)
	{
		command_queue->Signal((ID3D12Fence*)fence->GetHandle(), fence_value);
	}

	void D3D12CommandQueue::Wait(GfxFence& fence, uint64_t fence_value)
	{
		command_queue->Wait((ID3D12Fence*)fence->GetHandle(), fence_value);
	}
}
