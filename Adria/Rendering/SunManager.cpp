#include "SunManager.h"
#include "Math/MathCommon.h"
#include "Editor/GUICommand.h"

using namespace DirectX;

namespace adria
{
	void SunManager::Update(entt::registry& reg)
	{
		sun_entity = entt::null;
		moon_entity = entt::null;
		sun_light = nullptr;
		moon_light = nullptr;
		sun_transform = nullptr;
		moon_transform = nullptr;
		sun_active = false;
		sun_direction = Vector3(0.0f, -1.0f, 0.0f);

		for (entt::entity e : reg.view<Sun, Light, Transform>())
		{
			sun_entity = e;
			sun_light = &reg.get<Light>(e);
			sun_transform = &reg.get<Transform>(e);
			break;
		}
		for (entt::entity e : reg.view<Moon, Light, Transform>())
		{
			moon_entity = e;
			moon_light = &reg.get<Light>(e);
			moon_transform = &reg.get<Transform>(e);
			break;
		}

		if (sun_light)
		{
			Vector3 dir = Vector3(sun_light->direction);
			Float dir_len = dir.Length();
			if (dir_len > 1e-6f)
			{
				dir /= dir_len;
			}
			else
			{
				dir = Vector3(0.0f, -1.0f, 0.0f);
			}

			sun_active = dir.y < 0.0f;
			sun_direction = dir;
			sun_light->active = sun_active;
		}

		if (moon_light)
		{
			moon_light->active = !sun_active;
		}
	}

	void SunManager::FillFrameCBuffer(FrameCBuffer& cbuf)
	{
		Light* active_light = sun_active ? sun_light : moon_light;
		if (!active_light)
		{
			cbuf.sun_direction = Vector4(0.0f, 1.0f, 0.0f, 0.0f);
			cbuf.sun_color = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
			return;
		}

		Vector3 dir = Vector3(active_light->direction);
		dir.Normalize();
		cbuf.sun_direction = Vector4(-dir.x, -dir.y, -dir.z, 0.0f);

		if (sun_active)
		{
			Float sun_elevation = -sun_direction.y;
			Float sun_fade = Clamp(sun_elevation * 5.0f, 0.0f, 1.0f);
			cbuf.sun_color = active_light->color * active_light->intensity * sun_fade;
		}
		else
		{
			cbuf.sun_color = active_light->color * active_light->intensity;
		}
	}

	void SunManager::GUI(std::function<void()> on_changed)
	{
		QueueGUI([&, on_changed = std::move(on_changed)]()
			{
				if (ImGui::TreeNode("Sun Settings"))
				{
					if (sun_light)
					{
						static Float sun_elevation = 75.0f;
						static Float sun_azimuth = 260.0f;
						ConvertDirectionToAzimuthAndElevation(Vector4(-sun_light->direction.x, -sun_light->direction.y, -sun_light->direction.z, 0), sun_elevation, sun_azimuth);

						Bool changed = false;
						changed |= ImGui::ColorEdit3("Sun Color", &sun_light->color.x);
						changed |= ImGui::SliderFloat("Sun Energy", &sun_light->intensity, 0.0f, 50.0f);
						changed |= ImGui::SliderFloat("Sun Elevation", &sun_elevation, -90.0f, 90.0f);
						changed |= ImGui::SliderFloat("Sun Azimuth", &sun_azimuth, 0.0f, 360.0f);

						if (changed && on_changed)
						{
							on_changed();
						}
						sun_light->direction = ConvertElevationAndAzimuthToDirection(sun_elevation, sun_azimuth);
						sun_light->position = 1e3 * sun_light->direction;
						sun_light->direction = -sun_light->direction;
						sun_transform->local_transform = XMMatrixTranslationFromVector(sun_light->position);
					}
					ImGui::TreePop();
				}
				if (ImGui::TreeNode("Moon Settings"))
				{
					if (moon_light)
					{
						static Float moon_elevation = 45.0f;
						static Float moon_azimuth = 80.0f;
						ConvertDirectionToAzimuthAndElevation(Vector4(-moon_light->direction.x, -moon_light->direction.y, -moon_light->direction.z, 0), moon_elevation, moon_azimuth);

						Bool changed = false;
						changed |= ImGui::ColorEdit3("Moon Color", &moon_light->color.x);
						changed |= ImGui::SliderFloat("Moon Energy", &moon_light->intensity, 0.0f, 5.0f);
						changed |= ImGui::SliderFloat("Moon Elevation", &moon_elevation, 0.0f, 90.0f);
						changed |= ImGui::SliderFloat("Moon Azimuth", &moon_azimuth, 0.0f, 360.0f);

						if (changed && on_changed)
						{
							on_changed();
						}
						moon_light->direction = ConvertElevationAndAzimuthToDirection(moon_elevation, moon_azimuth);
						moon_light->position = 1e3 * moon_light->direction;
						moon_light->direction = -moon_light->direction;
						moon_transform->local_transform = XMMatrixTranslationFromVector(moon_light->position);
					}
					ImGui::TreePop();
				}
			}, GUICommandGroup_Renderer, GUICommandSubGroup_Environment);
	}
}
