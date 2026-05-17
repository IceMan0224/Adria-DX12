#include "VulkanRayTracingPipeline.h"
#include "VulkanDevice.h"
#include "Graphics/GfxShader.h"
#include "Logging/Log.h"

namespace adria
{
	namespace
	{
		ADRIA_LOG_CHANNEL(Graphics);

		// SPIR-V execution model constants for ray tracing
		enum class SpvExecutionModel : Uint32
		{
			RayGenerationKHR = 5313,
			IntersectionKHR  = 5314,
			AnyHitKHR        = 5315,
			ClosestHitKHR    = 5316,
			MissKHR          = 5317,
			CallableKHR      = 5318,
		};

		struct SpvEntryPoint
		{
			VkShaderStageFlagBits stage;
			std::string           name;
		};

		VkShaderStageFlagBits ExecutionModelToStage(Uint32 model)
		{
			switch (SpvExecutionModel(model))
			{
			case SpvExecutionModel::RayGenerationKHR: return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
			case SpvExecutionModel::IntersectionKHR:  return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
			case SpvExecutionModel::AnyHitKHR:        return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
			case SpvExecutionModel::ClosestHitKHR:    return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
			case SpvExecutionModel::MissKHR:          return VK_SHADER_STAGE_MISS_BIT_KHR;
			case SpvExecutionModel::CallableKHR:      return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
			default:                                  return VkShaderStageFlagBits(0);
			}
		}

		//Walks a SPIR-V blob and records every OpEntryPoint (opcode 15). The SPIR-V header is 5 words and each
		//instruction carries (wordCount << 16) | opcode in its first word. The entry name is a null-terminated
		//UTF-8 string packed into little-endian words
		std::vector<SpvEntryPoint> ReflectSpirvEntryPoints(void const* spirv_data, Uint64 spirv_size)
		{
			std::vector<SpvEntryPoint> result;
			if (spirv_size < 5 * sizeof(Uint32)) 
			{
				return result;
			}

			Uint32 const* words      = (Uint32 const*)spirv_data;
			Uint32        word_count = (Uint32)(spirv_size / sizeof(Uint32));
			static constexpr Uint32 SPIRV_MAGIC = 0x07230203u;
			if (words[0] != SPIRV_MAGIC) 
			{
				ADRIA_LOG(WARNING, "RT shader blob is not SPIR-V (magic mismatch)");
				return result;
			}

			Uint32 cursor = 5; 
			while (cursor < word_count)
			{
				Uint32 instr_header = words[cursor];
				Uint32 instr_size   = (instr_header >> 16) & 0xFFFF;
				Uint32 instr_opcode = instr_header & 0xFFFF;
				if (instr_size == 0 || cursor + instr_size > word_count) break;

				if (instr_opcode == 15 && instr_size >= 4)
				{
					Uint32 model    = words[cursor + 1];
					VkShaderStageFlagBits stage = ExecutionModelToStage(model);
					if (stage != 0)
					{
						Char const* name_bytes = (Char const*)&words[cursor + 3];
						Uint32 max_bytes = (instr_size - 3) * sizeof(Uint32);
						std::string name(name_bytes, strnlen(name_bytes, max_bytes));
						result.push_back({ stage, std::move(name) });
					}
				}
				cursor += instr_size;
			}
			return result;
		}
	}

	VulkanRayTracingPipeline::VulkanRayTracingPipeline(GfxDevice* gfx, GfxRayTracingPipelineDesc const& desc)
	{
		vk_device = static_cast<VulkanDevice*>(gfx);
		ADRIA_ASSERT(vk_device->IsRayTracingSupported());

		VkPhysicalDeviceRayTracingPipelinePropertiesKHR const& rt_props = vk_device->GetRayTracingPipelineProperties();
		handle_size      = rt_props.shaderGroupHandleSize;
		handle_alignment = rt_props.shaderGroupHandleAlignment;
		base_alignment   = rt_props.shaderGroupBaseAlignment;

		struct Stage { VkShaderStageFlagBits stage; std::string name; Uint32 module_index; };
		std::vector<Stage> stages;
		stages.reserve(desc.libraries.size() * 2);

		modules.reserve(desc.libraries.size());
		for (GfxRayTracingShaderLibrary const& library : desc.libraries)
		{
			ADRIA_ASSERT(library.shader != nullptr);
			if (library.shader == nullptr || library.shader->GetSize() == 0)
			{
				continue;
			}

			VkShaderModuleCreateInfo module_ci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
			module_ci.codeSize = library.shader->GetSize();
			module_ci.pCode    = (Uint32 const*)library.shader->GetData();
			VkShaderModule module = VK_NULL_HANDLE;
			VK_CHECK(vkCreateShaderModule(vk_device->GetVkDevice(), &module_ci, nullptr, &module));
			Uint32 module_index = (Uint32)modules.size();
			modules.push_back(module);

			std::vector<SpvEntryPoint> entries = ReflectSpirvEntryPoints(library.shader->GetData(), library.shader->GetSize());

			if (library.exports.empty())
			{
				for (SpvEntryPoint const& e : entries)
				{
					stages.push_back({ e.stage, e.name, module_index });
				}
			}
			else
			{
				for (std::string const& exp : library.exports)
				{
					auto it = std::find_if(entries.begin(), entries.end(),
						[&](SpvEntryPoint const& e) { return e.name == exp; });
					ADRIA_ASSERT(it != entries.end() && "RT library export not found in SPIR-V module");
					if (it != entries.end())
					{
						stages.push_back({ it->stage, it->name, module_index });
					}
				}
			}
		}

		std::unordered_map<std::string, Uint32> stage_index_by_name;
		stage_index_by_name.reserve(stages.size());
		std::vector<VkPipelineShaderStageCreateInfo> stage_infos;
		stage_infos.reserve(stages.size());
		for (Uint32 i = 0; i < (Uint32)stages.size(); ++i)
		{
			stage_index_by_name[stages[i].name] = i;
			VkPipelineShaderStageCreateInfo si{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
			si.stage  = stages[i].stage;
			si.module = modules[stages[i].module_index];
			si.pName  = stages[i].name.c_str();
			stage_infos.push_back(si);
		}

		std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
		groups.reserve(stages.size() + desc.hit_groups.size());
		auto NewGroup = [&]() -> VkRayTracingShaderGroupCreateInfoKHR& {
			VkRayTracingShaderGroupCreateInfoKHR g{ VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
			g.generalShader      = VK_SHADER_UNUSED_KHR;
			g.closestHitShader   = VK_SHADER_UNUSED_KHR;
			g.anyHitShader       = VK_SHADER_UNUSED_KHR;
			g.intersectionShader = VK_SHADER_UNUSED_KHR;
			groups.push_back(g);
			return groups.back();
		};

		std::vector<std::string> group_names; 
		group_names.reserve(stages.size() + desc.hit_groups.size());
		auto AddGeneralGroup = [&](VkShaderStageFlagBits wanted_stage)
		{
			for (Uint32 i = 0; i < (Uint32)stages.size(); ++i)
			{
				if (stages[i].stage != wanted_stage) 
				{
					continue;
				}
				VkRayTracingShaderGroupCreateInfoKHR& g = NewGroup();
				g.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
				g.generalShader = i;
				group_names.push_back(stages[i].name);
			}
		};
		AddGeneralGroup(VK_SHADER_STAGE_RAYGEN_BIT_KHR);
		AddGeneralGroup(VK_SHADER_STAGE_MISS_BIT_KHR);
		AddGeneralGroup(VK_SHADER_STAGE_CALLABLE_BIT_KHR);

		for (GfxRayTracingHitGroup const& hg : desc.hit_groups)
		{
			VkRayTracingShaderGroupCreateInfoKHR& g = NewGroup();
			g.type = hg.intersection_shader.empty()
				? VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR
				: VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;

			auto FindStage = [&](std::string const& name) -> Uint32
			{
				if (name.empty()) return VK_SHADER_UNUSED_KHR;
				auto it = stage_index_by_name.find(name);
				ADRIA_ASSERT(it != stage_index_by_name.end() && "Hit group references missing shader");
				return it != stage_index_by_name.end() ? it->second : VK_SHADER_UNUSED_KHR;
			};
			g.closestHitShader   = FindStage(hg.closest_hit_shader);
			g.anyHitShader       = FindStage(hg.any_hit_shader);
			g.intersectionShader = FindStage(hg.intersection_shader);
			group_names.push_back(hg.name);
		}

		VkRayTracingPipelineCreateInfoKHR pipeline_ci{ VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
		pipeline_ci.stageCount                   = (Uint32)stage_infos.size();
		pipeline_ci.pStages                      = stage_infos.data();
		pipeline_ci.groupCount                   = (Uint32)groups.size();
		pipeline_ci.pGroups                      = groups.data();
		pipeline_ci.maxPipelineRayRecursionDepth = std::min<Uint32>(desc.max_recursion_depth, rt_props.maxRayRecursionDepth);
		pipeline_ci.layout                       = vk_device->GetCommonPipelineLayout();

		VK_CHECK(pfn_vkCreateRayTracingPipelinesKHR(
			vk_device->GetVkDevice(),
			VK_NULL_HANDLE,
			VK_NULL_HANDLE,
			1,
			&pipeline_ci,
			nullptr,
			&pipeline));

		Uint32 group_count = (Uint32)groups.size();
		handle_storage.resize((Uint64)group_count * handle_size);
		if (group_count > 0)
		{
			VK_CHECK(pfn_vkGetRayTracingShaderGroupHandlesKHR(
				vk_device->GetVkDevice(),
				pipeline,
				0, group_count,
				handle_storage.size(),
				handle_storage.data()));
		}
		group_index_by_name.reserve(group_count);
		for (Uint32 i = 0; i < group_count; ++i)
		{
			if (!group_names[i].empty())
			{
				group_index_by_name.emplace(group_names[i], i);
			}
		}
	}

	VulkanRayTracingPipeline::~VulkanRayTracingPipeline()
	{
		VkDevice device = vk_device->GetVkDevice();
		if (pipeline != VK_NULL_HANDLE)
		{
			vkDestroyPipeline(device, pipeline, nullptr);
			pipeline = VK_NULL_HANDLE;
		}
		for (VkShaderModule m : modules)
		{
			if (m != VK_NULL_HANDLE) 
			{
				vkDestroyShaderModule(device, m, nullptr);
			}
		}
		modules.clear();
	}

	Bool VulkanRayTracingPipeline::HasShader(Char const* name) const
	{
		if (!name) 
		{
			return false;
		}
		return group_index_by_name.find(name) != group_index_by_name.end();
	}

	void const* VulkanRayTracingPipeline::GetShaderGroupHandle(Char const* name) const
	{
		if (!name) 
		{
			return nullptr;
		}
		auto it = group_index_by_name.find(name);
		if (it == group_index_by_name.end()) 
		{
			return nullptr;
		}
		return handle_storage.data() + (Uint64)it->second * handle_size;
	}
}
