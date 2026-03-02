#include "HierarchySystem.h"
#include "Components.h"

namespace adria
{
	static void PropagateTransformRecursive(entt::registry& reg, entt::entity entity, Matrix const& parent_world)
	{
		Transform* transform = reg.try_get<Transform>(entity);
		if (!transform) 
		{
			return;
		}

		transform->current_transform = transform->local_transform * parent_world;
		Relationship const* rel = reg.try_get<Relationship>(entity);
		if (rel)
		{
			for (entt::entity child : rel->children)
			{
				PropagateTransformRecursive(reg, child, transform->current_transform);
			}
		}
	}

	void PropagateTransforms(entt::registry& reg)
	{
		auto view = reg.view<Transform>();
		for (entt::entity e : view)
		{
			Relationship const* rel = reg.try_get<Relationship>(e);
			if (rel && rel->parent != entt::null)
			{
				continue; 
			}

			Transform& t = view.get<Transform>(e);
			t.current_transform = t.local_transform;
			if (rel)
			{
				for (entt::entity child : rel->children)
				{
					PropagateTransformRecursive(reg, child, t.current_transform);
				}
			}
		}

		auto node_view = reg.view<NodeMeshRef, Transform>();
		for (entt::entity e : node_view)
		{
			NodeMeshRef const& node_ref = node_view.get<NodeMeshRef>(e);
			Transform const& transform  = node_view.get<Transform>(e);
			Mesh* mesh = reg.try_get<Mesh>(node_ref.mesh_entity);
			if (!mesh)
			{
				continue;
			}

			for (Uint32 i = node_ref.first_instance_index; i < node_ref.first_instance_index + node_ref.instance_count; ++i)
			{
				if (i < (Uint32)mesh->instances.size())
				{
					mesh->instances[i].world_transform = transform.current_transform;
				}
			}
		}

		auto mat_view = reg.view<NodeMeshRef, Material>();
		for (entt::entity e : mat_view)
		{
			NodeMeshRef const& node_ref = mat_view.get<NodeMeshRef>(e);
			Material const& mat = mat_view.get<Material>(e);
			Mesh* mesh = reg.try_get<Mesh>(node_ref.mesh_entity);
			if (!mesh || node_ref.first_instance_index >= (Uint32)mesh->instances.size())
			{
				continue;
			}
			SubMeshInstance const& inst = mesh->instances[node_ref.first_instance_index];
			if (inst.submesh_index < (Uint32)mesh->submeshes.size())
			{
				Uint32 mat_idx = mesh->submeshes[inst.submesh_index].material_index;
				if (mat_idx < (Uint32)mesh->materials.size())
				{
					mesh->materials[mat_idx] = mat;
				}
			}
		}
	}

	void SetParent(entt::registry& reg, entt::entity child, entt::entity parent)
	{
		UnsetParent(reg, child);

		reg.get_or_emplace<Relationship>(child);
		reg.get_or_emplace<Relationship>(parent);

		reg.get<Relationship>(child).parent = parent;
		reg.get<Relationship>(parent).children.push_back(child);
	}

	void UnsetParent(entt::registry& reg, entt::entity child)
	{
		Relationship* child_rel = reg.try_get<Relationship>(child);
		if (!child_rel || child_rel->parent == entt::null) 
		{
			return;
		}

		entt::entity old_parent = child_rel->parent;
		Relationship* parent_rel = reg.try_get<Relationship>(old_parent);
		if (parent_rel)
		{
			auto& children = parent_rel->children;
			children.erase(std::remove(children.begin(), children.end(), child), children.end());
		}
		child_rel->parent = entt::null;
	}
}
