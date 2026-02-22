#include "ReflectionPassGroup.h"
#include "RayTracedReflectionsPass.h"
#include "SSRPass.h"
#include "Core/ConsoleManager.h"
#include "Editor/GUICommand.h"


namespace adria
{
	enum ReflectionType : Uint8
	{
		ReflectionType_None,
		ReflectionType_SSR,
		ReflectionType_RTR,
		ReflectionType_Count
	};

	static TAutoConsoleVariable<Int> Reflection("r.Reflections", ReflectionType_SSR, "0 - No Reflections, 1 - SSR, 2 - RTR");

	static Char const* ReflectionName[] = { "None", "SSR", "RTR" };

	ReflectionPassGroup::ReflectionPassGroup(GfxDevice* gfx, Uint32 width, Uint32 height) : reflection_type(ReflectionType_SSR)
	{
		post_effect_idx = static_cast<Uint32>(reflection_type);
		Reflection->AddOnChanged(ConsoleVariableDelegate::CreateLambda([this](IConsoleVariable* cvar)
			{
				reflection_type = static_cast<ReflectionType>(cvar->GetInt());
				post_effect_idx = static_cast<Uint32>(reflection_type);
			}));

		post_effects.resize(ReflectionType_Count);
		post_effects[ReflectionType_None] = std::make_unique<EmptyPostEffect>();
		post_effects[ReflectionType_SSR]  = std::make_unique<SSRPass>(gfx, width, height);
		post_effects[ReflectionType_RTR]  = std::make_unique<RayTracedReflectionsPass>(gfx, width, height);
		is_rtr_supported = post_effects[ReflectionType_RTR]->IsSupported();
	}

	void ReflectionPassGroup::GroupGUI()
	{
		QueueGUI([&]()
			{
				static Int current_reflection_type = (Int)reflection_type;
				if (ImGui::BeginCombo("Reflections", ReflectionName[current_reflection_type]))
				{
					for (Int i = 0; i < ReflectionType_Count; ++i)
					{
						Bool supported = post_effects[i]->IsSupported();
						ImGui::BeginDisabled(!supported);
						if (ImGui::Selectable(ReflectionName[i], i == current_reflection_type))
						{
							current_reflection_type = i;
							Reflection->Set(current_reflection_type);
						}
						ImGui::EndDisabled();
						if (!supported && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
						{
							ImGui::SetTooltip("Not supported on this backend");
						}
					}
					ImGui::EndCombo();
				}
			}, GUICommandGroup_PostProcessing, GUICommandSubGroup_Reflection);
	}

}
