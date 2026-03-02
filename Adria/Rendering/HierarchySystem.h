#pragma once
#include "entt/entity/registry.hpp"

namespace adria
{
	void PropagateTransforms(entt::registry& reg);
	void SetParent(entt::registry& reg, entt::entity child, entt::entity parent);
	void UnsetParent(entt::registry& reg, entt::entity child);
}
