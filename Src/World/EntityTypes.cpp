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

std::vector<const eg::IEntitySerializer*> entitySerializers;

void InitEntitySerializers()
{
	entitySerializers.push_back(ECEntrance::EntitySerializer);
	entitySerializers.push_back(WallLight::EntitySerializer);
	entitySerializers.push_back(GravitySwitch::EntitySerializer);
}
