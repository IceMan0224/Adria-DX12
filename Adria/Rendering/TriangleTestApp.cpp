#include "TriangleTestApp.h"
#include "Core/Paths.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommandList.h"
#include "Graphics/GfxBuffer.h"
#include "Graphics/GfxBufferView.h"
#include "Graphics/GfxTexture.h"
#include "Graphics/GfxPipelineState.h"
#include "Graphics/GfxShaderCompiler.h"
#include "Graphics/GfxRenderPass.h"
#include "Rendering/ShaderManager.h"
#include "Platform/Window.h"
#include "Platform/Input.h"

namespace adria
{
	struct TriangleTestApp::Impl
	{
		Window* window;
		std::unique_ptr<GfxDevice> gfx;
		std::unique_ptr<GfxGraphicsPipelineState> pso;
		std::unique_ptr<GfxBuffer> vertex_buffer;

		Impl(Window* window) : window(window)
		{
			gfx = CreateGfxDevice(GfxBackend::Vulkan, window);
			GfxShaderCompiler::Initialize(gfx.get());
			ShaderManager::Initialize();
			CreateVertexBuffer();
			CreatePSO();
		}

		~Impl()
		{
			gfx->WaitForGPU();
			pso.reset();
			vertex_buffer.reset();
			ShaderManager::Destroy();
			GfxShaderCompiler::Destroy();
		}

		void CreateVertexBuffer()
		{
			struct Vertex
			{
				Float x, y;
				Float r, g, b;
			};

			Vertex vertices[] =
			{
				{  0.0f,  0.5f,  1.0f, 0.0f, 0.0f },
				{  0.5f, -0.5f,  0.0f, 1.0f, 0.0f },
				{ -0.5f, -0.5f,  0.0f, 0.0f, 1.0f },
			};

			GfxBufferDesc desc = VertexBufferDesc(3, sizeof(Vertex), false);
			vertex_buffer = gfx->CreateBuffer(desc, GfxBufferData(vertices));
			vertex_buffer->SetName("TriangleVB");
		}

		void CreatePSO()
		{
			GfxGraphicsPipelineStateDesc desc{};
			desc.VS = VS_TriangleTest;
			desc.PS = PS_TriangleTest;
			desc.num_render_targets = 1;
			desc.rtv_formats[0] = gfx->GetBackbuffer()->GetFormat();
			desc.depth_state.depth_enable = false;
			desc.rasterizer_state.cull_mode = GfxCullMode::None;
			desc.topology_type = GfxPrimitiveTopologyType::Triangle;

			desc.input_layout.elements =
			{
				{ "POSITION", 0, GfxFormat::R32G32_FLOAT,   0, GfxInputLayout::APPEND_ALIGNED_ELEMENT, GfxInputClassification::PerVertexData },
				{ "COLOR",    0, GfxFormat::R32G32B32_FLOAT, 0, GfxInputLayout::APPEND_ALIGNED_ELEMENT, GfxInputClassification::PerVertexData },
			};

			pso = gfx->CreateManagedGraphicsPipelineState(desc);
		}

		void Run()
		{
			gfx->BeginFrame();

			GfxCommandList* cmd = gfx->GetGraphicsCommandList();
			GfxTexture* backbuffer = gfx->GetBackbuffer();

			cmd->TextureBarrier(*backbuffer, GfxResourceState::Present, GfxResourceState::RTV);
			cmd->FlushBarriers();

			GfxDescriptor rtv = gfx->CreateTextureRTV(backbuffer);

			GfxRenderPassDesc rp{};
			GfxColorAttachmentDesc color_att{};
			color_att.cpu_handle = rtv;
			color_att.beginning_access = GfxLoadAccessOp::Clear;
			color_att.ending_access = GfxStoreAccessOp::Preserve;
			color_att.clear_value = GfxClearValue(0.53f, 0.81f, 0.92f, 1.0f);
			rp.rtv_attachments.push_back(color_att);
			rp.width = backbuffer->GetWidth();
			rp.height = backbuffer->GetHeight();

			cmd->BeginRenderPass(rp);

			cmd->SetPipelineState(pso->Get());
			cmd->SetViewport(0, 0, backbuffer->GetWidth(), backbuffer->GetHeight());
			cmd->SetScissorRect(0, 0, backbuffer->GetWidth(), backbuffer->GetHeight());
			cmd->SetPrimitiveTopology(GfxPrimitiveTopology::TriangleList);

			GfxVertexBufferView vbv(vertex_buffer.get());
			cmd->SetVertexBuffer(vbv);
			cmd->Draw(3);

			cmd->EndRenderPass();

			cmd->TextureBarrier(*backbuffer, GfxResourceState::RTV, GfxResourceState::Present);
			cmd->FlushBarriers();

			gfx->EndFrame();
		}
	};

	TriangleTestApp::TriangleTestApp(Window* window) : pimpl(std::make_unique<Impl>(window)) {}
	TriangleTestApp::~TriangleTestApp() = default;

	void TriangleTestApp::Run()
	{
		pimpl->Run();
	}
}
