#include "AmbientOcclusionManager.h"
#include "Core/ConsoleManager.h"
#include "Graphics/GfxDevice.h"
#include "Editor/GUICommand.h"

namespace adria
{
	enum AmbientOcclusionType : Int
	{
		AmbientOcclusionType_None,
		AmbientOcclusionType_SSAO,
		AmbientOcclusionType_HBAO,
		AmbientOcclusionType_NNAO,
		AmbientOcclusionType_CACAO,
		AmbientOcclusionType_RTAO
	};
	static Char const* AOName[] = { "None", "SSAO", "HBAO", "NNAO", "CACAO", "RTAO" };

	static TAutoConsoleVariable<Int>  AmbientOcclusion("r.AmbientOcclusion", AmbientOcclusionType_SSAO, "0 - No AO, 1 - SSAO, 2 - HBAO, 3 - NNAO, 4 - CACAO, 5 - RTAO");

	AmbientOcclusionManager::AmbientOcclusionManager(GfxDevice* gfx, Uint32 width, Uint32 height)
		: gfx(gfx), ssao_pass(gfx, width, height), hbao_pass(gfx, width, height), nnao_pass(gfx, width, height),
		  rtao_pass(gfx, width, height), cacao_pass(gfx, width, height)
	{
	}

	AmbientOcclusionManager::~AmbientOcclusionManager()
	{
	}

	void AmbientOcclusionManager::OnResize(Uint32 w, Uint32 h)
	{
		ssao_pass.OnResize(w, h);
		hbao_pass.OnResize(w, h);
		nnao_pass.OnResize(w, h);
		cacao_pass.OnResize(w, h);
		rtao_pass.OnResize(w, h);
	}

	void AmbientOcclusionManager::OnSceneInitialized()
	{
		ssao_pass.OnSceneInitialized();
		hbao_pass.OnSceneInitialized();
		nnao_pass.OnSceneInitialized();
	}

	void AmbientOcclusionManager::AddPass(RenderGraph& rg)
	{
		switch (AmbientOcclusion.Get())
		{
		case AmbientOcclusionType_SSAO:  ssao_pass.AddPass(rg); break;
		case AmbientOcclusionType_HBAO:  hbao_pass.AddPass(rg); break;
		case AmbientOcclusionType_NNAO:  nnao_pass.AddPass(rg); break;
		case AmbientOcclusionType_CACAO: cacao_pass.AddPass(rg); break;
		case AmbientOcclusionType_RTAO:  rtao_pass.AddPass(rg); break;
		}
	}

	void AmbientOcclusionManager::GUI()
	{
		QueueGUI([&]()
			{
				Bool ao_supported[] =
				{
					true,
					true,
					true,
					true,
					cacao_pass.IsSupported(),
					rtao_pass.IsSupported(),
				};
				Int current_ao = AmbientOcclusion.Get();
				if (ImGui::BeginCombo("Ambient Occlusion Type", AOName[current_ao]))
				{
					for (Int i = 0; i < 6; ++i)
					{
						ImGui::BeginDisabled(!ao_supported[i]);
						if (ImGui::Selectable(AOName[i], i == current_ao))
						{
							AmbientOcclusion->Set(i);
						}
						ImGui::EndDisabled();
						if (!ao_supported[i] && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
						{
							ImGui::SetTooltip("Not supported on this backend");
						}
					}
					ImGui::EndCombo();
				}
			}, GUICommandGroup_PostProcessing, GUICommandSubGroup_AO);

		switch (AmbientOcclusion.Get())
		{
		case AmbientOcclusionType_SSAO:  ssao_pass.GUI();  break;
		case AmbientOcclusionType_HBAO:  hbao_pass.GUI();  break;
		case AmbientOcclusionType_NNAO:  nnao_pass.GUI();  break;
		case AmbientOcclusionType_CACAO: cacao_pass.GUI(); break;
		case AmbientOcclusionType_RTAO:  rtao_pass.GUI();  break;
		}
	}

}
