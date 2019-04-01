#include "EntityTypes.hpp"
#include "Entities/WallLight.hpp"
#include "Entities/Entrance.hpp"
#include "Entities/GravitySwitch.hpp"

std::vector<SpawnableEntityType> spawnableEntityTypes = 
{
	SpawnableEntityType { &WallLight::CreateEntity, "Wall Light" },
	SpawnableEntityType { &ECEntrance::CreateEntity, "Entrance/Exit" },
	SpawnableEntityType { &GravitySwitch::CreateEntity, "Gravity Switch" }
};
