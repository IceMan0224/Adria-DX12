#include "GIManager.h"
#include "Core/ConsoleManager.h"
#include "Graphics/GfxDevice.h"
#include "Editor/GUICommand.h"

namespace adria
{
	enum GITechnique : Int
	{
		GITechnique_None,
		GITechnique_DDGI,
		GITechnique_ReSTIR_GI
	};
	static Char const* GITechniqueName[] = { "None", "DDGI", "ReSTIR GI" };

	static TAutoConsoleVariable<Int> GITechniqueCVar("r.GI.Technique", GITechnique_DDGI, "Global illumination technique: 0 - None, 1 - DDGI, 2 - ReSTIR GI");

	GIManager::GIManager(GfxDevice* gfx, entt::registry& reg, Uint32 width, Uint32 height)
		: gfx(gfx), ddgi(gfx, reg, width, height), restir_gi(gfx, width, height)
	{
	}

	void GIManager::OnResize(Uint32 w, Uint32 h)
	{
		ddgi.OnResize(w, h);
		restir_gi.OnResize(w, h);
	}

	void GIManager::OnSceneInitialized()
	{
		ddgi.OnSceneInitialized();
	}

	void GIManager::AddPasses(RenderGraph& rg)
	{
		Int technique = GITechniqueCVar.Get();
		if (technique == GITechnique_ReSTIR_GI && restir_gi.IsSupported())
		{
			restir_gi.AddPasses(rg);
		}
		else
		{
			restir_gi.InvalidateHistory();
		}
		if (technique == GITechnique_DDGI && ddgi.IsSupported())
		{
			ddgi.AddPasses(rg);
		}
	}

	void GIManager::AddVisualizePass(RenderGraph& rg)
	{
		if (GITechniqueCVar.Get() == GITechnique_DDGI && ddgi.IsSupported() && ddgi.Visualize())
		{
			ddgi.AddVisualizePass(rg);
		}
	}

	Int32 GIManager::GetDDGIVolumeIndex()
	{
		Bool ddgi_active = GITechniqueCVar.Get() == GITechnique_DDGI && ddgi.IsSupported();
		return ddgi_active ? ddgi.GetDDGIVolumeIndex() : -1;
	}

	void GIManager::GUI()
	{
		QueueGUI([&]()
			{
				Bool technique_supported[] = { true, ddgi.IsSupported(), restir_gi.IsSupported() };
				Int current_technique = GITechniqueCVar.Get();
				if (ImGui::BeginCombo("Global Illumination", GITechniqueName[current_technique]))
				{
					for (Int i = 0; i < IM_ARRAYSIZE(GITechniqueName); ++i)
					{
						ImGui::BeginDisabled(!technique_supported[i]);
						if (ImGui::Selectable(GITechniqueName[i], i == current_technique))
						{
							GITechniqueCVar->Set(i);
						}
						ImGui::EndDisabled();
						if (!technique_supported[i] && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
						{
							ImGui::SetTooltip("Not supported on this backend");
						}
					}
					ImGui::EndCombo();
				}
			}, GUICommandGroup_Renderer, GUICommandSubGroup_Lighting);

		switch (GITechniqueCVar.Get())
		{
		case GITechnique_DDGI:      if (ddgi.IsSupported())      ddgi.GUI();      break;
		case GITechnique_ReSTIR_GI: if (restir_gi.IsSupported()) restir_gi.GUI(); break;
		}
	}

}
