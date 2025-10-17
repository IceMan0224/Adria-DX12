#pragma once
#include "GfxPipelineStateFwd.h"

namespace adria
{
	class GfxDevice;
	class GfxShader;

	struct GfxRayTracingShaderLibrary
	{
		GfxShader const* shader = nullptr;
		std::vector<std::string> exports;

		GfxRayTracingShaderLibrary() = default;
		explicit GfxRayTracingShaderLibrary(GfxShader const* _shader)
			: shader(_shader), exports()
		{
		}

		GfxRayTracingShaderLibrary(GfxShader const* _shader, std::vector<std::string> _exports)
			: shader(_shader), exports(std::move(_exports))
		{
		}
	};

	struct GfxRayTracingHitGroup
	{
		std::string name;
		std::string closest_hit_shader;
		std::string any_hit_shader;
		std::string intersection_shader;

		GfxRayTracingHitGroup() = default;

		GfxRayTracingHitGroup(std::string _name, std::string _closest_hit, std::string _any_hit, std::string _intersection)
			: name(std::move(_name))
			, closest_hit_shader(std::move(_closest_hit))
			, any_hit_shader(std::move(_any_hit))
			, intersection_shader(std::move(_intersection))
		{
		}

		static GfxRayTracingHitGroup Triangle(std::string const& name, std::string const& closest_hit = "", std::string const& any_hit = "")
		{
			return GfxRayTracingHitGroup(name, closest_hit, any_hit, "");
		}

		static GfxRayTracingHitGroup Procedural(std::string const& name, std::string const& intersection, std::string const& closest_hit = "", std::string const& any_hit = "")
		{
			return GfxRayTracingHitGroup(name, closest_hit, any_hit, intersection);
		}
	};

	struct GfxRayTracingLocalRootSignatureAssociation
	{
		GfxRootSignatureID root_signature = GfxRootSignatureID::Invalid;
		std::vector<std::string> shader_names;

		GfxRayTracingLocalRootSignatureAssociation() = default;

		GfxRayTracingLocalRootSignatureAssociation(GfxRootSignatureID _root_signature, std::vector<std::string> const& _shader_names)
			: root_signature(_root_signature)
			, shader_names(_shader_names)
		{
		}
	};

	struct GfxRayTracingPipelineDesc
	{
		std::vector<GfxRayTracingShaderLibrary> libraries;
		std::vector<GfxRayTracingHitGroup> hit_groups;
		Uint32 max_payload_size = 0;
		Uint32 max_attribute_size = 8;
		Uint32 max_recursion_depth = 1;
		GfxRootSignatureID global_root_signature = GfxRootSignatureID::Invalid;
		std::vector<GfxRayTracingLocalRootSignatureAssociation> local_root_signatures;

		GfxRayTracingPipelineDesc() = default;
	};

	class GfxRayTracingPipeline
	{
	public:
		virtual ~GfxRayTracingPipeline() = default;

		virtual Bool IsValid() const = 0;
		virtual void* GetNative() const = 0;
		virtual Bool HasShader(Char const* name) const = 0;

	protected:
		GfxRayTracingPipeline() = default;
		ADRIA_NONCOPYABLE(GfxRayTracingPipeline)
	};
}