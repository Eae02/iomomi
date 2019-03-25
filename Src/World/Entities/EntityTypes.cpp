#include "SpotLightEntity.hpp"
#include "EntranceEntity.hpp"
#include "PointLightEntity.hpp"
#include "GravitySwitchEntity.hpp"
#include "LightProbeEntity.hpp"
#include "WallLightEntity.hpp"

void InitEntityTypes()
{
	DefineEntityType(EntityType::Make<SpotLightEntity>("SpotLight", "Spot Light"));
	DefineEntityType(EntityType::Make<PointLightEntity>("PointLight", "Point Light"));
	DefineEntityType(EntityType::Make<EntranceEntity>("Entrance", "Entrance"));
	DefineEntityType(EntityType::Make<GravitySwitchEntity>("GravitySwitch", "Gravity Switch"));
	DefineEntityType(EntityType::Make<LightProbeEntity>("LightProbe", "Light Probe"));
	DefineEntityType(EntityType::Make<WallLightEntity>("WallLight", "Wall Light"));
}
