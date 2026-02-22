#include "DepthOfFieldPassGroup.h"
#include "DepthOfFieldPass.h"
#include "FFXDepthOfFieldPass.h"
#include "Core/ConsoleManager.h"
#include "Editor/GUICommand.h"

namespace adria
{
	static TAutoConsoleVariable<Int> DepthOfField("r.DepthOfField", 0, "0 - No Depth of Field, 1 - Custom, 2 - FFX");

	enum class DepthOfFieldType : Uint8
	{
		None,
		Custom,
		FFX,
		Count
	};

	static Char const* DepthOfFieldName[] = { "None", "Custom", "FFX" };

	DepthOfFieldPassGroup::DepthOfFieldPassGroup(GfxDevice* gfx, Uint32 width, Uint32 height) : depth_of_field_type(DepthOfFieldType::None)
	{
		post_effect_idx = static_cast<Uint32>(depth_of_field_type);
		DepthOfField->AddOnChanged(ConsoleVariableDelegate::CreateLambda([this](IConsoleVariable* cvar) 
		{ 
			depth_of_field_type = static_cast<DepthOfFieldType>(cvar->GetInt()); 
			post_effect_idx = static_cast<Uint32>(depth_of_field_type);
		}));
		using enum DepthOfFieldType;
		post_effects.resize((Uint32)Count);
		post_effects[(Uint32)None] = std::make_unique<EmptyPostEffect>();
		post_effects[(Uint32)Custom] = std::make_unique<DepthOfFieldPass>(gfx, width, height);
		post_effects[(Uint32)FFX] = std::make_unique<FFXDepthOfFieldPass>(gfx, width, height);
	}

	void DepthOfFieldPassGroup::GroupGUI()
	{
		QueueGUI([&]()
			{
				static Int current_depth_of_field_type = (Int)depth_of_field_type;
				if (ImGui::BeginCombo("Depth of Field Type", DepthOfFieldName[current_depth_of_field_type]))
				{
					for (Int i = 0; i < (Int)DepthOfFieldType::Count; ++i)
					{
						Bool supported = post_effects[i]->IsSupported();
						ImGui::BeginDisabled(!supported);
						if (ImGui::Selectable(DepthOfFieldName[i], i == current_depth_of_field_type))
						{
							current_depth_of_field_type = i;
							depth_of_field_type = static_cast<DepthOfFieldType>(current_depth_of_field_type);
							DepthOfField->Set(current_depth_of_field_type);
						}
						ImGui::EndDisabled();
						if (!supported && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
						{
							ImGui::SetTooltip("Not supported on this backend");
						}
					}
					ImGui::EndCombo();
				}
			}, GUICommandGroup_PostProcessing, GUICommandSubGroup_DepthOfField
		);
	}

}

