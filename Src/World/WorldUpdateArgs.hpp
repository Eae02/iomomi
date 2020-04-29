#pragma once

#include <functional>

struct WorldUpdateArgs
{
	float dt;
	class World* world;
	const class PhysicsEngine* physicsEngine;
	class Player* player;
	class WaterSimulator* waterSim;
	std::function<void(const eg::Sphere&)> invalidateShadows;
};
