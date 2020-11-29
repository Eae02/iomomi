#pragma once

#include <functional>

enum class WorldMode
{
	Game,
	Editor,
	Thumbnail
};

struct WorldUpdateArgs
{
	float dt;
	WorldMode mode;
	class World* world;
	const class PhysicsEngine* physicsEngine;
	class Player* player;
	class WaterSimulator* waterSim;
	std::function<void(const eg::Sphere&)> invalidateShadows;
};
