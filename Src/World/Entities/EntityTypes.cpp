#include "EntityTypes.hpp"
#include "Entity.hpp"

#include "EntTypes/EntranceExitEnt.hpp"
#include "EntTypes/WallLightEnt.hpp"
#include "EntTypes/DecalEnt.hpp"
#include "EntTypes/GooPlaneEnt.hpp"
#include "EntTypes/WaterPlaneEnt.hpp"
#include "EntTypes/GravitySwitchEnt.hpp"
#include "EntTypes/FloorButtonEnt.hpp"
#include "EntTypes/ActivationLightStripEnt.hpp"
#include "EntTypes/CubeEnt.hpp"
#include "EntTypes/CubeSpawnerEnt.hpp"
#include "EntTypes/PlatformEnt.hpp"
#include "EntTypes/ForceFieldEnt.hpp"
#include "EntTypes/GravityBarrierEnt.hpp"
#include "EntTypes/RampEnt.hpp"

const EntTypeID entUpdateOrder[NUM_UPDATABLE_ENTITY_TYPES] =
{
	EntTypeID::ActivationLightStrip,
	EntTypeID::FloorButton,
	EntTypeID::ForceField,
	EntTypeID::GravityBarrier,
	EntTypeID::CubeSpawner,
	EntTypeID::Cube,
	EntTypeID::Platform
};

template <typename T>
void DefineEntityType(std::string name, std::string prettyName)
{
	bool ok = entTypeMap.emplace(T::TypeID, EntType {
		T::EntFlags,
		std::move(name),
		std::move(prettyName),
		T::EditorIconIndex,
		&Ent::CreateCallback<T>,
		&CloneEntity<T>
	}).second;
	if (!ok)
	{
		EG_PANIC("Entity " << name << " using occupied type id!");
	}
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
}
