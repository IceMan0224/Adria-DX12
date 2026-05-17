#include "VulkanPipelineState.h"
#include "VulkanDevice.h"
#include "VulkanConversions.h"
#include "Rendering/ShaderManager.h"

namespace adria
{
	static VkShaderModule CreateShaderModule(VkDevice device, GfxShaderKey const& key, std::string& out_entry_point)
	{
		if (!key.IsValid())
		{
			return VK_NULL_HANDLE;
		}

		GfxShader const& shader = SM_GetGfxShader(key);
		if (shader.GetSize() == 0)
		{
			return VK_NULL_HANDLE;
		}

		out_entry_point = shader.GetDesc().entry_point;

		VkShaderModuleCreateInfo ci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		ci.codeSize = shader.GetSize();
		ci.pCode    = (Uint32 const*)shader.GetData();

		VkShaderModule module = VK_NULL_HANDLE;
		VK_CHECK(vkCreateShaderModule(device, &ci, nullptr, &module));
		return module;
	}

	static void FillDepthStencilOp(VkStencilOpState& out, GfxDepthStencilState::GfxDepthStencilOp const& in)
	{
		out.failOp      = ConvertStencilOp(in.stencil_fail_op);
		out.passOp      = ConvertStencilOp(in.stencil_pass_op);
		out.depthFailOp = ConvertStencilOp(in.stencil_depth_fail_op);
		out.compareOp   = ConvertCompareOp(in.stencil_func);
		out.compareMask = 0xff;
		out.writeMask   = 0xff;
		out.reference   = 0;
	}

	VulkanPipelineState::VulkanPipelineState(GfxDevice* gfx, GfxGraphicsPipelineStateDesc const& desc)
		: type(GfxPipelineStateType::Graphics)
	{
		VulkanDevice* vk_gfx = static_cast<VulkanDevice*>(gfx);
		device = vk_gfx->GetVkDevice();
		VkPipelineLayout layout = vk_gfx->GetCommonPipelineLayout();

		std::string vs_entry, ps_entry, hs_entry, ds_entry, gs_entry;
		VkShaderModule vs_module = CreateShaderModule(device, desc.VS, vs_entry);
		VkShaderModule ps_module = CreateShaderModule(device, desc.PS, ps_entry);
		VkShaderModule hs_module = CreateShaderModule(device, desc.HS, hs_entry);
		VkShaderModule ds_module = CreateShaderModule(device, desc.DS, ds_entry);
		VkShaderModule gs_module = CreateShaderModule(device, desc.GS, gs_entry);

		std::vector<VkPipelineShaderStageCreateInfo> stages;
		auto AddStage = [&](VkShaderModule mod, VkShaderStageFlagBits stage_flag, Char const* entry)
		{
			if (mod == VK_NULL_HANDLE) 
			{ 
				return;
			}
			VkPipelineShaderStageCreateInfo s{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
			s.stage  = stage_flag;
			s.module = mod;
			s.pName  = entry;
			stages.push_back(s);
		};
		AddStage(vs_module, VK_SHADER_STAGE_VERTEX_BIT, vs_entry.c_str());
		AddStage(hs_module, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, hs_entry.c_str());
		AddStage(ds_module, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, ds_entry.c_str());
		AddStage(gs_module, VK_SHADER_STAGE_GEOMETRY_BIT, gs_entry.c_str());
		AddStage(ps_module, VK_SHADER_STAGE_FRAGMENT_BIT, ps_entry.c_str());

		std::vector<VkVertexInputBindingDescription>   bindings;
		std::vector<VkVertexInputAttributeDescription> attributes;
		std::vector<Uint32> binding_offsets(16, 0);
		for (Uint32 i = 0; i < (Uint32)desc.input_layout.elements.size(); ++i)
		{
			auto const& elem = desc.input_layout.elements[i];
			Bool binding_exists = false;
			for (auto const& b : bindings)
			{
				if (b.binding == elem.input_slot) 
				{ 
					binding_exists = true; 
					break; 
				}
			}
			if (!binding_exists)
			{
				VkVertexInputBindingDescription binding{};
				binding.binding   = elem.input_slot;
				binding.stride    = 0;
				binding.inputRate = elem.input_slot_class == GfxInputClassification::PerInstanceData
				                  ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
				bindings.push_back(binding);
			}

			VkVertexInputAttributeDescription attr{};
			attr.location = i;
			attr.binding  = elem.input_slot;
			attr.format   = ConvertFormat(elem.format);
			if (elem.aligned_byte_offset == GfxInputLayout::APPEND_ALIGNED_ELEMENT)
			{
				attr.offset = binding_offsets[elem.input_slot];
			}
			else
			{
				attr.offset = elem.aligned_byte_offset;
			}
			binding_offsets[elem.input_slot] = attr.offset + GetGfxFormatStride(elem.format);
			attributes.push_back(attr);
		}

		for (VkVertexInputBindingDescription& b : bindings)
		{
			b.stride = binding_offsets[b.binding];
		}

		VkPipelineVertexInputStateCreateInfo vertex_input{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
		vertex_input.vertexBindingDescriptionCount   = (Uint32)bindings.size();
		vertex_input.pVertexBindingDescriptions      = bindings.data();
		vertex_input.vertexAttributeDescriptionCount = (Uint32)attributes.size();
		vertex_input.pVertexAttributeDescriptions    = attributes.data();

		VkPipelineInputAssemblyStateCreateInfo input_assembly{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
		input_assembly.topology               = ConvertTopology(GfxPrimitiveTopology::TriangleList);
		input_assembly.primitiveRestartEnable = VK_FALSE;

		VkPipelineTessellationStateCreateInfo tessellation{ VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };
		tessellation.patchControlPoints = 1;

		VkPipelineViewportStateCreateInfo viewport{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
		viewport.viewportCount = 1;
		viewport.scissorCount  = 1;

		auto const& rs = desc.rasterizer_state;
		VkPipelineRasterizationStateCreateInfo raster{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
		raster.depthClampEnable        = VK_FALSE;
		raster.rasterizerDiscardEnable = VK_FALSE;
		raster.polygonMode             = ConvertFillMode(rs.fill_mode);
		raster.cullMode                = ConvertCullMode(rs.cull_mode);
		raster.frontFace               = rs.front_counter_clockwise ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
		raster.depthBiasEnable         = rs.depth_bias != 0 ? VK_TRUE : VK_FALSE;
		raster.depthBiasConstantFactor = (Float)rs.depth_bias;
		raster.depthBiasClamp          = rs.depth_bias_clamp;
		raster.depthBiasSlopeFactor    = rs.slope_scaled_depth_bias;
		raster.lineWidth               = 1.0f;

		VkPipelineMultisampleStateCreateInfo multisample{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
		multisample.rasterizationSamples = ConvertSampleCount(desc.sample_mask == UINT_MAX ? 1 : 1);
		multisample.sampleShadingEnable  = VK_FALSE;

		auto const& dss = desc.depth_state;
		VkPipelineDepthStencilStateCreateInfo depth_stencil{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
		depth_stencil.depthTestEnable       = dss.depth_enable ? VK_TRUE : VK_FALSE;
		depth_stencil.depthWriteEnable      = dss.depth_write_mask == GfxDepthWriteMask::All ? VK_TRUE : VK_FALSE;
		depth_stencil.depthCompareOp        = ConvertCompareOp(dss.depth_func);
		depth_stencil.depthBoundsTestEnable = VK_FALSE;
		depth_stencil.stencilTestEnable     = dss.stencil_enable ? VK_TRUE : VK_FALSE;
		FillDepthStencilOp(depth_stencil.front, dss.front_face);
		FillDepthStencilOp(depth_stencil.back, dss.back_face);
		depth_stencil.front.compareMask = dss.stencil_read_mask;
		depth_stencil.front.writeMask   = dss.stencil_write_mask;
		depth_stencil.back.compareMask  = dss.stencil_read_mask;
		depth_stencil.back.writeMask    = dss.stencil_write_mask;

		auto const& bs = desc.blend_state;
		std::array<VkPipelineColorBlendAttachmentState, 8> blend_attachments{};
		for (Uint32 i = 0; i < desc.num_render_targets; ++i)
		{
			auto const& rt = bs.render_target[bs.independent_blend_enable ? i : 0];
			auto& att = blend_attachments[i];
			att.blendEnable         = rt.blend_enable ? VK_TRUE : VK_FALSE;
			att.srcColorBlendFactor = ConvertBlendFactor(rt.src_blend);
			att.dstColorBlendFactor = ConvertBlendFactor(rt.dest_blend);
			att.colorBlendOp        = ConvertBlendOp(rt.blend_op);
			att.srcAlphaBlendFactor = ConvertBlendFactor(rt.src_blend_alpha);
			att.dstAlphaBlendFactor = ConvertBlendFactor(rt.dest_blend_alpha);
			att.alphaBlendOp        = ConvertBlendOp(rt.blend_op_alpha);
			att.colorWriteMask      = ConvertColorWrite(rt.render_target_write_mask);
		}

		VkPipelineColorBlendStateCreateInfo blend{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
		blend.logicOpEnable   = VK_FALSE;
		blend.attachmentCount = desc.num_render_targets;
		blend.pAttachments    = blend_attachments.data();

		std::vector<VkDynamicState> dynamic_states = 
		{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
			VK_DYNAMIC_STATE_STENCIL_REFERENCE,
			VK_DYNAMIC_STATE_BLEND_CONSTANTS,
			VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
		};

		VkPipelineDynamicStateCreateInfo dynamic_state{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
		dynamic_state.dynamicStateCount = (Uint32)dynamic_states.size();
		dynamic_state.pDynamicStates    = dynamic_states.data();

		std::vector<VkFormat> color_formats;
		color_formats.reserve(desc.num_render_targets);
		for (Uint32 i = 0; i < desc.num_render_targets; ++i)
		{
			color_formats.push_back(ConvertFormat(desc.rtv_formats[i]));
		}

		VkPipelineRenderingCreateInfo rendering{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
		rendering.colorAttachmentCount    = (Uint32)color_formats.size();
		rendering.pColorAttachmentFormats = color_formats.data();
		rendering.depthAttachmentFormat   = desc.dsv_format != GfxFormat::UNKNOWN ? ConvertFormat(desc.dsv_format) : VK_FORMAT_UNDEFINED;

		Bool has_stencil = desc.dsv_format == GfxFormat::D24_UNORM_S8_UINT || desc.dsv_format == GfxFormat::D32_FLOAT_S8X24_UINT;
		rendering.stencilAttachmentFormat = has_stencil ? ConvertFormat(desc.dsv_format) : VK_FORMAT_UNDEFINED;

		VkGraphicsPipelineCreateInfo ci{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
		ci.pNext               = &rendering;
		ci.stageCount          = (Uint32)stages.size();
		ci.pStages             = stages.data();
		ci.pVertexInputState   = &vertex_input;
		ci.pInputAssemblyState = &input_assembly;
		ci.pTessellationState  = (hs_module != VK_NULL_HANDLE) ? &tessellation : nullptr;
		ci.pViewportState      = &viewport;
		ci.pRasterizationState = &raster;
		ci.pMultisampleState   = &multisample;
		ci.pDepthStencilState  = &depth_stencil;
		ci.pColorBlendState    = &blend;
		ci.pDynamicState       = &dynamic_state;
		ci.layout              = layout;
		ci.renderPass          = VK_NULL_HANDLE;

		VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline));

		if (vs_module) { vkDestroyShaderModule(device, vs_module, nullptr); }
		if (ps_module) { vkDestroyShaderModule(device, ps_module, nullptr); }
		if (hs_module) { vkDestroyShaderModule(device, hs_module, nullptr); }
		if (ds_module) { vkDestroyShaderModule(device, ds_module, nullptr); }
		if (gs_module) { vkDestroyShaderModule(device, gs_module, nullptr); }
	}

	VulkanPipelineState::VulkanPipelineState(GfxDevice* gfx, GfxComputePipelineStateDesc const& desc)
		: type(GfxPipelineStateType::Compute)
	{
		VulkanDevice* vk_gfx = static_cast<VulkanDevice*>(gfx);
		device = vk_gfx->GetVkDevice();

		std::string cs_entry;
		VkShaderModule cs_module = CreateShaderModule(device, desc.CS, cs_entry);
		ADRIA_ASSERT(cs_module != VK_NULL_HANDLE);

		VkPipelineShaderStageCreateInfo stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
		stage.module = cs_module;
		stage.pName  = cs_entry.c_str();

		VkComputePipelineCreateInfo ci{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
		ci.stage  = stage;
		ci.layout = vk_gfx->GetCommonPipelineLayout();

		VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline));
		vkDestroyShaderModule(device, cs_module, nullptr);
	}

	VulkanPipelineState::VulkanPipelineState(GfxDevice* gfx, GfxMeshShaderPipelineStateDesc const& desc)
		: type(GfxPipelineStateType::MeshShader)
	{
		VulkanDevice* vk_gfx = static_cast<VulkanDevice*>(gfx);
		device = vk_gfx->GetVkDevice();
		VkPipelineLayout layout = vk_gfx->GetCommonPipelineLayout();

		std::string as_entry, ms_entry, ps_entry;
		VkShaderModule as_module = CreateShaderModule(device, desc.AS, as_entry);
		VkShaderModule ms_module = CreateShaderModule(device, desc.MS, ms_entry);
		VkShaderModule ps_module = CreateShaderModule(device, desc.PS, ps_entry);

		std::vector<VkPipelineShaderStageCreateInfo> stages;
		auto AddStage = [&](VkShaderModule mod, VkShaderStageFlagBits stage_flag, Char const* entry)
		{
			if (mod == VK_NULL_HANDLE) 
			{ 
				return; 
			}
			VkPipelineShaderStageCreateInfo s{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
			s.stage  = stage_flag;
			s.module = mod;
			s.pName  = entry;
			stages.push_back(s);
		};
		AddStage(as_module, VK_SHADER_STAGE_TASK_BIT_EXT, as_entry.c_str());
		AddStage(ms_module, VK_SHADER_STAGE_MESH_BIT_EXT, ms_entry.c_str());
		AddStage(ps_module, VK_SHADER_STAGE_FRAGMENT_BIT, ps_entry.c_str());

		auto const& rs = desc.rasterizer_state;
		VkPipelineRasterizationStateCreateInfo raster{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
		raster.polygonMode          = ConvertFillMode(rs.fill_mode);
		raster.cullMode             = ConvertCullMode(rs.cull_mode);
		raster.frontFace            = rs.front_counter_clockwise ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
		raster.depthBiasEnable      = rs.depth_bias != 0 ? VK_TRUE : VK_FALSE;
		raster.depthBiasConstantFactor = (Float)rs.depth_bias;
		raster.depthBiasClamp       = rs.depth_bias_clamp;
		raster.depthBiasSlopeFactor = rs.slope_scaled_depth_bias;
		raster.lineWidth            = 1.0f;

		auto const& dss = desc.depth_state;
		VkPipelineDepthStencilStateCreateInfo depth_stencil{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
		depth_stencil.depthTestEnable  = dss.depth_enable ? VK_TRUE : VK_FALSE;
		depth_stencil.depthWriteEnable = dss.depth_write_mask == GfxDepthWriteMask::All ? VK_TRUE : VK_FALSE;
		depth_stencil.depthCompareOp   = ConvertCompareOp(dss.depth_func);

		auto const& bs = desc.blend_state;
		std::array<VkPipelineColorBlendAttachmentState, 8> blend_attachments{};
		for (Uint32 i = 0; i < desc.num_render_targets; ++i)
		{
			auto const& rt = bs.render_target[bs.independent_blend_enable ? i : 0];
			auto& att = blend_attachments[i];
			att.blendEnable         = rt.blend_enable ? VK_TRUE : VK_FALSE;
			att.srcColorBlendFactor = ConvertBlendFactor(rt.src_blend);
			att.dstColorBlendFactor = ConvertBlendFactor(rt.dest_blend);
			att.colorBlendOp        = ConvertBlendOp(rt.blend_op);
			att.srcAlphaBlendFactor = ConvertBlendFactor(rt.src_blend_alpha);
			att.dstAlphaBlendFactor = ConvertBlendFactor(rt.dest_blend_alpha);
			att.alphaBlendOp        = ConvertBlendOp(rt.blend_op_alpha);
			att.colorWriteMask      = ConvertColorWrite(rt.render_target_write_mask);
		}

		VkPipelineColorBlendStateCreateInfo blend{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
		blend.attachmentCount = desc.num_render_targets;
		blend.pAttachments    = blend_attachments.data();

		VkPipelineViewportStateCreateInfo viewport{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
		viewport.viewportCount = 1;
		viewport.scissorCount  = 1;

		VkPipelineMultisampleStateCreateInfo multisample{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
		multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		std::vector<VkDynamicState> dynamic_states = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
			VK_DYNAMIC_STATE_STENCIL_REFERENCE,
		};
		VkPipelineDynamicStateCreateInfo dynamic_state{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
		dynamic_state.dynamicStateCount = (Uint32)dynamic_states.size();
		dynamic_state.pDynamicStates    = dynamic_states.data();

		std::vector<VkFormat> color_formats;
		for (Uint32 i = 0; i < desc.num_render_targets; ++i)
		{
			color_formats.push_back(ConvertFormat(desc.rtv_formats[i]));
		}
		VkPipelineRenderingCreateInfo rendering{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
		rendering.colorAttachmentCount    = (Uint32)color_formats.size();
		rendering.pColorAttachmentFormats = color_formats.data();
		rendering.depthAttachmentFormat   = desc.dsv_format != GfxFormat::UNKNOWN ? ConvertFormat(desc.dsv_format) : VK_FORMAT_UNDEFINED;

		VkGraphicsPipelineCreateInfo ci{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
		ci.pNext               = &rendering;
		ci.stageCount          = (Uint32)stages.size();
		ci.pStages             = stages.data();
		ci.pRasterizationState = &raster;
		ci.pDepthStencilState  = &depth_stencil;
		ci.pColorBlendState    = &blend;
		ci.pViewportState      = &viewport;
		ci.pMultisampleState   = &multisample;
		ci.pDynamicState       = &dynamic_state;
		ci.layout              = layout;

		VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline));

		if (as_module) { vkDestroyShaderModule(device, as_module, nullptr); }
		if (ms_module) { vkDestroyShaderModule(device, ms_module, nullptr); }
		if (ps_module) { vkDestroyShaderModule(device, ps_module, nullptr); }
	}

	VulkanPipelineState::~VulkanPipelineState()
	{
		if (pipeline != VK_NULL_HANDLE)
		{
			vkDestroyPipeline(device, pipeline, nullptr);
		}
	}
}
