#include "EntityTypes.hpp"
#include "Entities/WallLight.hpp"
#include "Entities/Entrance.hpp"
#include "Entities/Cube.hpp"
#include "Entities/GravitySwitch.hpp"
#include "Entities/ECFloorButton.hpp"

std::vector<SpawnableEntityType> spawnableEntityTypes = 
{
	SpawnableEntityType { &WallLight::CreateEntity, "Wall Light" },
	SpawnableEntityType { &ECEntrance::CreateEntity, "Entrance/Exit" },
	SpawnableEntityType { &GravitySwitch::CreateEntity, "Gravity Switch" },
	SpawnableEntityType { &Cube::CreateEntity, "Cube" },
	SpawnableEntityType { &ECFloorButton::CreateEntity, "Floor Button" }
};

std::vector<const eg::IEntitySerializer*> entitySerializers;

void InitEntitySerializers()
{
	entitySerializers.push_back(ECEntrance::EntitySerializer);
	entitySerializers.push_back(WallLight::EntitySerializer);
	entitySerializers.push_back(GravitySwitch::EntitySerializer);
	entitySerializers.push_back(Cube::EntitySerializer);
	entitySerializers.push_back(ECFloorButton::EntitySerializer);
}
