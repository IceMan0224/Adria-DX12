#include "D3D12Swapchain.h"
#include "D3D12Device.h"
#include "D3D12Conversions.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommandQueue.h"
#include "Graphics/GfxCommandList.h"
#include "Graphics/GfxTexture.h"
#include "Platform/Window.h"

namespace adria
{

    D3D12Swapchain::D3D12Swapchain(GfxDevice* gfx, GfxSwapchainDesc const& desc)
		: gfx(gfx), width(desc.width), height(desc.height)
	{
		DXGI_SWAP_CHAIN_DESC1 swapchain_desc{};
		swapchain_desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
		swapchain_desc.BufferCount = GFX_BACKBUFFER_COUNT;
		swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapchain_desc.Format = ConvertGfxFormat(desc.backbuffer_format);
		swapchain_desc.Width = width;
		swapchain_desc.Height = height;
		swapchain_desc.Scaling = DXGI_SCALING_NONE;
		swapchain_desc.Stereo = FALSE;
		swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapchain_desc.SampleDesc.Count = 1;
		swapchain_desc.SampleDesc.Quality = 0;

		DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen_desc{};
		fullscreen_desc.RefreshRate.Denominator = 60;
		fullscreen_desc.RefreshRate.Numerator = 1;
		fullscreen_desc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		fullscreen_desc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		fullscreen_desc.Windowed = desc.fullscreen_windowed;

		GfxCommandQueue* graphics_queue = gfx->GetGraphicsCommandQueue();
		Ref<IDXGISwapChain1> swapchain1 = nullptr;

        D3D12Device* d3d12gfx = (D3D12Device*)gfx;

		GFX_CHECK_CALL(d3d12gfx->GetFactory()->CreateSwapChainForHwnd(
			(ID3D12CommandQueue*)graphics_queue->GetNative(),
			static_cast<HWND>(gfx->GetWindowHandle()),
			&swapchain_desc,
			&fullscreen_desc,
			nullptr,
			swapchain1.GetAddressOf()));

		swapchain.Reset();
		swapchain1.As(&swapchain);
		
		backbuffer_index = swapchain->GetCurrentBackBufferIndex();
		CreateBackbuffers();
	}

	D3D12Swapchain::~D3D12Swapchain() {}

	void D3D12Swapchain::SetAsRenderTarget(GfxCommandList* cmd_list)
	{
		GfxDescriptor rtvs[] = { GetBackbufferDescriptor() };
		cmd_list->SetRenderTargets(rtvs);
	}

	void D3D12Swapchain::ClearBackbuffer(GfxCommandList* cmd_list)
	{
		constexpr Float clear_color[] = { 0,0,0,0 };
		GfxDescriptor rtv = GetBackbufferDescriptor();
		cmd_list->ClearRenderTarget(rtv, clear_color);
	}

	Bool D3D12Swapchain::Present(Bool vsync)
	{
		HRESULT hr = swapchain->Present(vsync, 0);
		backbuffer_index = swapchain->GetCurrentBackBufferIndex();
		return SUCCEEDED(hr);
	}

	void D3D12Swapchain::OnResize(Uint32 w, Uint32 h)
	{
		width = w;
		height = h;

		for (Uint32 i = 0; i < GFX_BACKBUFFER_COUNT; ++i)
		{
			back_buffers[i].reset(nullptr);
		}

		DXGI_SWAP_CHAIN_DESC desc{};
		swapchain->GetDesc(&desc);
		HRESULT hr = swapchain->ResizeBuffers(desc.BufferCount, width, height, desc.BufferDesc.Format, desc.Flags);
		GFX_CHECK_CALL(hr);
		
		backbuffer_index = swapchain->GetCurrentBackBufferIndex();
		CreateBackbuffers();
	}

	void D3D12Swapchain::CreateBackbuffers()
	{
		for (Uint32 i = 0; i < GFX_BACKBUFFER_COUNT; ++i)
		{
			Ref<ID3D12Resource> backbuffer = nullptr;
			HRESULT hr = swapchain->GetBuffer(i, IID_PPV_ARGS(backbuffer.GetAddressOf()));
			GFX_CHECK_CALL(hr);
			D3D12_RESOURCE_DESC desc = backbuffer->GetDesc();
			GfxTextureDesc gfx_desc{};
			gfx_desc.width = (Uint32)desc.Width;
			gfx_desc.height = (Uint32)desc.Height;
			gfx_desc.format = ConvertDXGIFormat(desc.Format);
			gfx_desc.initial_state = GfxResourceState::Present;
			gfx_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);
			gfx_desc.bind_flags = GfxBindFlag::RenderTarget;
			back_buffers[i] = gfx->CreateBackbufferTexture(gfx_desc, backbuffer);
			back_buffers[i]->SetName("Backbuffer");
			backbuffer_rtvs[i] = gfx->CreateTextureRTV(back_buffers[i].get());
		}
	}

	GfxDescriptor D3D12Swapchain::GetBackbufferDescriptor() const
	{
		return backbuffer_rtvs[backbuffer_index];
	}
}
