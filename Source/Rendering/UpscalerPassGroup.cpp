#include "UpscalerPassGroup.h"
#include "FSR2Pass.h"
#include "FSR3Pass.h"
#include "XeSS2Pass.h"
#include "DLSS3Pass.h"
#include "MetalFXPass.h"
#include "Core/ConsoleManager.h"
#include "Editor/GUICommand.h"

namespace adria
{
	ADRIA_LOG_CHANNEL(PostProcessor);

	static TAutoConsoleVariable<Int>  Upscaler("r.Upscaler", 0, "0 - No Upscaler, 1 - FSR2, 2 - FSR3, 3 - XeSS2, 4 - DLSS3, 5 - MetalFX");

	enum class UpscalerType : Uint8
	{
		None,
		FSR2,
		FSR3,
		XeSS2,
		DLSS3,
		MetalFX,
		Count
	};
	Char const* UpscalerName[] =
	{
		"None",
		"FSR2",
		"FSR3",
		"XeSS2",
		"DLSS3",
		"MetalFX",
	};

	UpscalerPassGroup::UpscalerPassGroup(GfxDevice* gfx, Uint32 width, Uint32 height) : upscaler_type(UpscalerType::None), display_width(width), display_height(height)
	{
		post_effect_idx = static_cast<Uint32>(upscaler_type);
		Upscaler->AddOnChanged(ConsoleVariableDelegate::CreateLambda([this](IConsoleVariable* cvar)
			{
				upscaler_type = static_cast<UpscalerType>(cvar->GetInt());
				post_effect_idx = static_cast<Uint32>(upscaler_type);
			}));

		using enum UpscalerType;
		post_effects.resize((Uint32)Count);
		post_effects[(Uint32)None]    = std::make_unique<DummyUpscalerPass>();
		post_effects[(Uint32)FSR2]    = std::make_unique<FSR2Pass>(gfx, width, height);
		post_effects[(Uint32)FSR3]    = std::make_unique<FSR3Pass>(gfx, width, height);
		post_effects[(Uint32)XeSS2]   = std::make_unique<XeSS2Pass>(gfx, width, height);
		post_effects[(Uint32)DLSS3]   = std::make_unique<DLSS3Pass>(gfx, width, height);
		post_effects[(Uint32)MetalFX] = std::make_unique<MetalFXPass>(gfx, width, height);
	}

	void UpscalerPassGroup::OnResize(Uint32 w, Uint32 h)
	{
		display_width = w, display_height = h;
		if (upscaler_type != UpscalerType::None)
		{
			post_effects[(Uint32)upscaler_type]->OnResize(display_width, display_height);
		}
		else
		{
			upscaler_disabled_event.Broadcast(display_width, display_height);
		}
	}

	Bool UpscalerPassGroup::NeedsJitter() const
	{
		if (upscaler_type == UpscalerType::None) 
		{
			return false;
		}
		return post_effects[(Uint32)upscaler_type]->NeedsJitter();
	}

	void UpscalerPassGroup::GroupGUI()
	{
		QueueGUI([&]()
			{
				static Int current_upscaler = (Int)upscaler_type;
				if (ImGui::BeginCombo("Upscaler", UpscalerName[current_upscaler]))
				{
					for (Int i = 0; i < (Int)UpscalerType::Count; ++i)
					{
						Bool supported = post_effects[i]->IsSupported();
						ImGui::BeginDisabled(!supported);
						if (ImGui::Selectable(UpscalerName[i], i == current_upscaler))
						{
							current_upscaler = i;
							upscaler_type = static_cast<UpscalerType>(current_upscaler);
							Upscaler->Set(current_upscaler);
							if (upscaler_type != UpscalerType::None)
							{
								post_effects[(Uint32)upscaler_type]->OnResize(display_width, display_height);
							}
							else
							{
								upscaler_disabled_event.Broadcast(display_width, display_height);
							}
						}
						ImGui::EndDisabled();
						if (!supported && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
						{
							ImGui::SetTooltip("Not supported on this backend");
						}
					}
					ImGui::EndCombo();
				}
			}, GUICommandGroup_PostProcessing, GUICommandSubGroup_Upscaler);
	}
}
