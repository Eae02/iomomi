#pragma once

#include "../Dir.hpp"

struct ECWallMounted
{
	Dir wallUp;
	
	static glm::mat4 GetTransform(const eg::Entity& entity, float scale);
	
	static eg::AABB GetAABB(const eg::Entity& entity, float scale, float upDist);
};
