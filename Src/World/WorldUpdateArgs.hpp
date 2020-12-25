#pragma once

#include <functional>

enum class WorldMode
{
	Game,
	Editor,
	Thumbnail,
	Menu
};

struct WorldUpdateArgs
{
	float dt;
	WorldMode mode;
	class World* world;
	class PointLightShadowMapper* plShadowMapper;
	const class PhysicsEngine* physicsEngine;
	class Player* player;
	class WaterSimulator* waterSim;
};
