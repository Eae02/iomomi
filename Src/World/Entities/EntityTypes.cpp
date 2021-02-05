#include "EntityTypes.hpp"
#include "Entity.hpp"

#include "EntTypes/EntranceExitEnt.hpp"
#include "EntTypes/Visual/WallLightEnt.hpp"
#include "EntTypes/Visual/PointLightEnt.hpp"
#include "EntTypes/Visual/DecalEnt.hpp"
#include "EntTypes/GooPlaneEnt.hpp"
#include "EntTypes/WaterPlaneEnt.hpp"
#include "EntTypes/GravitySwitchEnt.hpp"
#include "EntTypes/Activation/FloorButtonEnt.hpp"
#include "EntTypes/Activation/ActivationLightStripEnt.hpp"
#include "EntTypes/Activation/CubeEnt.hpp"
#include "EntTypes/Activation/CubeSpawnerEnt.hpp"
#include "EntTypes/PlatformEnt.hpp"
#include "EntTypes/ForceFieldEnt.hpp"
#include "EntTypes/GravityBarrierEnt.hpp"
#include "EntTypes/Visual/RampEnt.hpp"
#include "EntTypes/Visual/WindowEnt.hpp"
#include "EntTypes/Visual/MeshEnt.hpp"
#include "EntTypes/WaterWallEnt.hpp"
#include "EntTypes/SlidingWallEnt.hpp"
#include "EntTypes/LadderEnt.hpp"

const EntTypeID entUpdateOrder[NUM_UPDATABLE_ENTITY_TYPES] =
{
	EntTypeID::ActivationLightStrip,
	EntTypeID::FloorButton,
	EntTypeID::ForceField,
	EntTypeID::GravityBarrier,
	EntTypeID::CubeSpawner,
	EntTypeID::Cube,
	EntTypeID::Platform,
	EntTypeID::GravitySwitch,
	EntTypeID::SlidingWall,
	EntTypeID::EntranceExit,
	EntTypeID::WaterWall
};

extern std::optional<EntType> entityTypes[(size_t)EntTypeID::MAX];

template <typename T>
void DefineEntityType(std::string name, std::string prettyName)
{
	if ((int)T::TypeID < 0 || (int)T::TypeID >= (int)EntTypeID::MAX)
	{
		EG_PANIC("Entity " << name << " using out of range id!");
	}
	if (entityTypes[(int)T::TypeID].has_value())
	{
		EG_PANIC("Entity " << name << " using occupied type id!");
	}
	
	entityTypes[((int)T::TypeID)] = EntType {
		T::EntFlags,
		std::move(name),
		std::move(prettyName),
		&Ent::CreateCallback<T>,
		&CloneEntity<T>
	};
}

void InitEntityTypes()
{
	DefineEntityType<EntranceExitEnt>("Entrance", "Entrance / Exit");
	DefineEntityType<WallLightEnt>("WallLight", "Wall Light");
	DefineEntityType<DecalEnt>("Decal", "Decal");
	DefineEntityType<GooPlaneEnt>("GooPlane", "Goo Plane");
	DefineEntityType<WaterPlaneEnt>("WaterPlane", "Water Plane");
	DefineEntityType<GravitySwitchEnt>("GravitySwitch", "Gravity Switch");
	DefineEntityType<FloorButtonEnt>("FloorButton", "Floor Button");
	DefineEntityType<ActivationLightStripEnt>("ActivationLightStrip", "Activation Light Strip");
	DefineEntityType<CubeEnt>("Cube", "Cube");
	DefineEntityType<CubeSpawnerEnt>("CubeSpawner", "Cube Spawner");
	DefineEntityType<PlatformEnt>("Platform", "Platform");
	DefineEntityType<ForceFieldEnt>("ForceField", "Force Field");
	DefineEntityType<GravityBarrierEnt>("GravityBarrier", "Gravity Barrier");
	DefineEntityType<RampEnt>("Ramp", "Ramp");
	DefineEntityType<WindowEnt>("Window", "Window");
	DefineEntityType<WaterWallEnt>("WaterWall", "Water Wall");
	DefineEntityType<SlidingWallEnt>("SlidingWall", "Sliding Wall");
	DefineEntityType<LadderEnt>("Ladder", "Ladder");
	DefineEntityType<PointLightEnt>("PointLight", "Point Light");
	DefineEntityType<MeshEnt>("Mesh", "Mesh");
}
