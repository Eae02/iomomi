#include "EntityTypes.hpp"
#include "Entity.hpp"

#include "EntTypes/EntranceExitEnt.hpp"
#include "EntTypes/WallLightEnt.hpp"
#include "EntTypes/DecalEnt.hpp"
#include "EntTypes/GooPlaneEnt.hpp"
#include "EntTypes/WaterPlaneEnt.hpp"
#include "EntTypes/GravitySwitchEnt.hpp"

template <typename T>
void DefineEntityType(std::string name, std::string prettyName)
{
	bool ok = entTypeMap.emplace(T::TypeID, EntType {
		T::EntFlags,
		std::move(name),
		std::move(prettyName),
		&Ent::CreateCallback<T>
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
}
