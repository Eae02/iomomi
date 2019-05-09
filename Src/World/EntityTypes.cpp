#include "EntityTypes.hpp"
#include "Entities/WallLight.hpp"
#include "Entities/Entrance.hpp"
#include "Entities/Cube.hpp"
#include "Entities/GravitySwitch.hpp"
#include "Entities/ECFloorButton.hpp"
#include "Entities/GooPlane.hpp"
#include "Entities/ECDecal.hpp"
#include "Entities/ECPlatform.hpp"

std::vector<SpawnableEntityType> spawnableEntityTypes = 
{
	SpawnableEntityType { &WallLight::CreateEntity, "Wall Light" },
	SpawnableEntityType { &ECEntrance::CreateEntity, "Entrance/Exit" },
	SpawnableEntityType { &GravitySwitch::CreateEntity, "Gravity Switch" },
	SpawnableEntityType { &Cube::CreateEntity, "Cube" },
	SpawnableEntityType { &ECFloorButton::CreateEntity, "Floor Button" },
	SpawnableEntityType { &GooPlane::CreateEntity, "Goo" },
	SpawnableEntityType { &ECDecal::CreateEntity, "Decal" },
	SpawnableEntityType { &ECPlatform::CreateEntity, "Platform" }
};

std::vector<const eg::IEntitySerializer*> entitySerializers;

void InitEntitySerializers()
{
	entitySerializers.push_back(ECEntrance::EntitySerializer);
	entitySerializers.push_back(WallLight::EntitySerializer);
	entitySerializers.push_back(GravitySwitch::EntitySerializer);
	entitySerializers.push_back(Cube::EntitySerializer);
	entitySerializers.push_back(GooPlane::EntitySerializer);
	entitySerializers.push_back(ECFloorButton::EntitySerializer);
	entitySerializers.push_back(ECDecal::EntitySerializer);
	entitySerializers.push_back(ECPlatform::EntitySerializer);
}
