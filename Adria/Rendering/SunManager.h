#pragma once
#include <functional>
#include "Components.h"
#include "ShaderStructs.h"
#include "Utilities/Singleton.h"
#include "entt/entity/registry.hpp"

namespace adria
{
	class SunManager : public Singleton<SunManager>
	{
		friend class Singleton<SunManager>;
	public:

		void Update(entt::registry& reg);
		void FillFrameCBuffer(FrameCBuffer& cbuf);
		void GUI(std::function<void()> on_changed = {});

		Bool IsSunActive() const { return sun_active; }
		Vector3 GetSunDirection() const { return sun_direction; }
		entt::entity GetSunEntity() const { return sun_entity; }
		entt::entity GetMoonEntity() const { return moon_entity; }
		entt::entity GetActiveCelestialEntity() const { return sun_active ? sun_entity : moon_entity; }

	private:
		SunManager() = default;

		entt::entity sun_entity = entt::null;
		entt::entity moon_entity = entt::null;
		Light* sun_light = nullptr;
		Light* moon_light = nullptr;
		Transform* sun_transform = nullptr;
		Transform* moon_transform = nullptr;
		Bool sun_active = true;
		Vector3 sun_direction{ 0.0f, -1.0f, 0.0f };
	};
#define g_SunManager SunManager::Get()
}
