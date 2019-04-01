#pragma once

#include <functional>

struct WorldUpdateArgs
{
	float dt;
	class World* world;
	class Player* player;
	std::function<void(const eg::Sphere&)> invalidateShadows;
};
