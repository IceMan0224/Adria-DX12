#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include "Core/Engine.h"
#include "Core/FatalAssert.h"
#include "Core/CommandLineOptions.h"
#include "Platform/Input.h"
#include "Platform/Window.h"
#include "Logging/FileSink.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxTexture.h"
#include "Graphics/GfxStates.h"
#include "Graphics/GfxFormat.h"
#include "Graphics/GfxRenderPass.h"
#include "Graphics/GfxBufferView.h"
#include "Graphics/GfxInputLayout.h"
#include "Graphics/GfxPipelineState.h"
#include "Graphics/GfxShaderCompiler.h"
#include "Rendering/ShaderManager.h"
#include "Editor/ImGuiManager.h"
#include "Utilities/CLIParser.h"

using namespace adria;

ADRIA_LOG_CHANNEL(Graphics);

class TriangleApp
{
public:
    TriangleApp(Window* _window) : window(_window)
    {
        device = CreateGfxDevice(GfxBackend::Metal, window);
        if (!device)
        {
            ADRIA_LOG(ERROR, "Failed to create graphics device!");
            return;
        }

        GfxShaderCompiler::Initialize();
        ShaderManager::Initialize();

        imgui_manager = CreateImguiManager(device.get());

        window->GetWindowEvent().AddLambda([this](WindowEventInfo const& msg_data) {
            if (imgui_manager)
            {
                imgui_manager->OnWindowEvent(msg_data);
            }
        });

        CreateVertexBuffer();
        CreatePipelineState();

        ADRIA_LOG(INFO, "Triangle app initialized successfully");
    }

    ~TriangleApp()
    {
        ShaderManager::Destroy();
        GfxShaderCompiler::Destroy();
    }

    void Render()
    {
        if (!vertex_buffer || !gfx_pipeline_state)
        {
            return;
        }

        device->BeginFrame();
        GfxTexture* backbuffer = device->GetBackbuffer();
        if (!backbuffer)
        {
            device->EndFrame();
            return;
        }

        imgui_manager->Begin();

        auto cmd_list = device->CreateCommandList(GfxCommandListType::Graphics);
        cmd_list->Begin();

        GfxDescriptor rtv_descriptor = device->CreateTextureRTV(backbuffer);

        // Use backbuffer's physical dimensions (includes Retina scaling)
        GfxTextureDesc const& backbuffer_desc = backbuffer->GetDesc();

        GfxRenderPassDesc render_pass_desc{};
        render_pass_desc.width = backbuffer_desc.width;
        render_pass_desc.height = backbuffer_desc.height;

        GfxColorAttachmentDesc color_attachment{};
        color_attachment.cpu_handle = rtv_descriptor;
        color_attachment.beginning_access = GfxLoadAccessOp::Clear;
        color_attachment.ending_access = GfxStoreAccessOp::Preserve;
        color_attachment.clear_value = GfxClearValue(0.2f, 0.3f, 0.4f, 1.0f);
        render_pass_desc.rtv_attachments.push_back(color_attachment);

        cmd_list->BeginRenderPass(render_pass_desc);

        cmd_list->SetPipelineState(gfx_pipeline_state.get());

        GfxVertexBufferView vbv(vertex_buffer.get());
        cmd_list->SetVertexBuffer(vbv, 0);

        cmd_list->SetPrimitiveTopology(GfxPrimitiveTopology::TriangleList);
        cmd_list->Draw(3, 1, 0, 0);

        imgui_manager->End(cmd_list.get());

        cmd_list->EndRenderPass();
        cmd_list->End();
        cmd_list->Submit();
        device->EndFrame();
    }

private:

    void CreateVertexBuffer()
    {
        struct Vertex 
        {
            float pos[2];
            float color[3];
        };

        Vertex vertices[] = {
            { { 0.0f,  0.5f}, {1.0f, 0.0f, 0.0f} },  // Top - Red
            { { 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f} },  // Right - Green
            { {-0.5f, -0.5f}, {0.0f, 0.0f, 1.0f} }   // Left - Blue
        };

        GfxBufferDesc buffer_desc{};
        buffer_desc.size = sizeof(vertices);
        buffer_desc.resource_usage = GfxResourceUsage::Default;
        buffer_desc.bind_flags = GfxBindFlag::None;  

        vertex_buffer = device->CreateBuffer(buffer_desc, GfxBufferData(vertices));
        vertex_buffer->SetName("TriangleVertexBuffer");
    }

    void CreatePipelineState()
    {
        GfxGraphicsPipelineStateDesc pso_desc{};
        pso_desc.num_render_targets = 1;
        pso_desc.rtv_formats[0] = GfxFormat::B8G8R8A8_UNORM;
        pso_desc.topology_type = GfxPrimitiveTopologyType::Triangle;
        pso_desc.depth_state.depth_enable = false;
        pso_desc.depth_state.depth_write_mask = GfxDepthWriteMask::Zero;
        pso_desc.VS = VS_TriangleTest;
        pso_desc.PS = PS_TriangleTest;

        pso_desc.input_layout.elements.push_back(GfxInputLayout::GfxInputElement{
            "POSITION", 0, GfxFormat::R32G32_FLOAT, 0, 0, GfxInputClassification::PerVertexData
        });
        pso_desc.input_layout.elements.push_back(GfxInputLayout::GfxInputElement{
            "COLOR", 0, GfxFormat::R32G32B32_FLOAT, 0, 8, GfxInputClassification::PerVertexData
        });

        gfx_pipeline_state = device->CreateGraphicsPipelineState(pso_desc);

        if (!gfx_pipeline_state)
        {
            ADRIA_LOG(ERROR, "Failed to create graphics pipeline state");
        }
        else
        {
            ADRIA_LOG(INFO, "Pipeline state created successfully");
        }
    }

    Window* window;
    std::unique_ptr<GfxDevice> device;
    std::unique_ptr<ImGuiManager> imgui_manager;
    std::unique_ptr<GfxBuffer> vertex_buffer;
    std::unique_ptr<GfxPipelineState> gfx_pipeline_state;
};

int main(int argc, char* argv[])
{
    @autoreleasepool
    {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        CommandLineOptions::Initialize(argc, argv);

        std::string log_file = CommandLineOptions::GetLogFile();
        LogLevel log_level = static_cast<LogLevel>(CommandLineOptions::GetLogLevel());
        ADRIA_SINK(FileSink, log_file.c_str(), log_level);

        WindowCreationParams window_params{};
        window_params.width = CommandLineOptions::GetWindowWidth();
        window_params.height = CommandLineOptions::GetWindowHeight();
        window_params.maximize = CommandLineOptions::GetMaximizeWindow();
        std::string window_title = CommandLineOptions::GetWindowTitle();
        window_params.title = window_title.c_str();

        Window window(window_params);
        g_Input.Initialize(&window);

        TriangleApp triangle_app(&window);

        [NSApp activateIgnoringOtherApps:YES];
        while (window.Loop())
        {
            @autoreleasepool
            {
                triangle_app.Render();
            }
        }
    }

    return 0;
}
