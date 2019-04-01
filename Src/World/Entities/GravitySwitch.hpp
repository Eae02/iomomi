#pragma once

namespace GravitySwitch
{
	extern eg::EntitySignature EntitySignature;
	
	eg::Entity* CreateEntity(eg::EntityManager& entityManager);
	
	eg::AABB GetAABB(const eg::Entity& entity);
	
	void InitFromYAML(eg::Entity& entity, const YAML::Node& node);
}
