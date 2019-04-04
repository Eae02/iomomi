#pragma once

#include "ECWallMounted.hpp"

namespace GravitySwitch
{
	static constexpr float SCALE = 1.0f;
	
	extern eg::EntitySignature EntitySignature;
	
	eg::Entity* CreateEntity(eg::EntityManager& entityManager);
	
	inline eg::AABB GetAABB(const eg::Entity& entity)
	{
		return ECWallMounted::GetAABB(entity, SCALE, 0.2f);
	}
	
	extern eg::IEntitySerializer* EntitySerializer;
}
