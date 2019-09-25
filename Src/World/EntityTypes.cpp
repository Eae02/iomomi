#include "EntityTypes.hpp"
#include "Entities/WallLight.hpp"
#include "Entities/Entrance.hpp"
#include "Entities/Cube.hpp"
#include "Entities/GravitySwitch.hpp"
#include "Entities/ECFloorButton.hpp"
#include "Entities/GooPlane.hpp"
#include "Entities/WaterPlane.hpp"
#include "Entities/ECDecal.hpp"
#include "Entities/ECPlatform.hpp"
#include "Entities/GravityBarrier.hpp"
#include "Entities/CubeSpawner.hpp"
#include "Entities/ForceField.hpp"

std::vector<SpawnableEntityType> spawnableEntityTypes = 
{
	SpawnableEntityType { &WallLight::CreateEntity, "Wall Light" },
	SpawnableEntityType { &ECEntrance::CreateEntity, "Entrance/Exit" },
	SpawnableEntityType { &GravitySwitch::CreateEntity, "Gravity Switch" },
	SpawnableEntityType { &Cube::CreateEntity, "Cube" },
	SpawnableEntityType { &CubeSpawner::CreateEntity, "Cube Spawner" },
	SpawnableEntityType { &ECFloorButton::CreateEntity, "Floor Button" },
	SpawnableEntityType { &GooPlane::CreateEntity, "Goo" },
	SpawnableEntityType { &WaterPlane::CreateEntity, "Water" },
	SpawnableEntityType { &ECDecal::CreateEntity, "Decal" },
	SpawnableEntityType { &ECPlatform::CreateEntity, "Platform" },
	SpawnableEntityType { &ECGravityBarrier::CreateEntity, "Barrier" },
	SpawnableEntityType { &ECForceField::CreateEntity, "Force Field" }
};

std::vector<const eg::IEntitySerializer*> entitySerializers;

void InitEntitySerializers()
{
	entitySerializers.push_back(ECEntrance::EntitySerializer);
	entitySerializers.push_back(WallLight::EntitySerializer);
	entitySerializers.push_back(GravitySwitch::EntitySerializer);
	entitySerializers.push_back(Cube::EntitySerializer);
	entitySerializers.push_back(CubeSpawner::EntitySerializer);
	entitySerializers.push_back(GooPlane::EntitySerializer);
	entitySerializers.push_back(WaterPlane::EntitySerializer);
	entitySerializers.push_back(ECFloorButton::EntitySerializer);
	entitySerializers.push_back(ECDecal::EntitySerializer);
	entitySerializers.push_back(ECPlatform::EntitySerializer);
	entitySerializers.push_back(ECGravityBarrier::EntitySerializer);
	entitySerializers.push_back(ECForceField::EntitySerializer);
}
