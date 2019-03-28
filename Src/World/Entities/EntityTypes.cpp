#include "SpotLightEntity.hpp"
#include "EntranceEntity.hpp"
#include "GravitySwitchEntity.hpp"
#include "LightProbeEntity.hpp"
#include "WallLightEntity.hpp"
#include "CubeEntity.hpp"

void InitEntityTypes()
{
	DefineEntityType(EntityType::Make<SpotLightEntity>("SpotLight", "Spot Light"));
	DefineEntityType(EntityType::Make<EntranceEntity>("Entrance", "Entrance"));
	DefineEntityType(EntityType::Make<GravitySwitchEntity>("GravitySwitch", "Gravity Switch"));
	DefineEntityType(EntityType::Make<LightProbeEntity>("LightProbe", "Light Probe"));
	DefineEntityType(EntityType::Make<WallLightEntity>("WallLight", "Wall Light"));
	DefineEntityType(EntityType::Make<CubeEntity>("Cube", "Cube"));
}
