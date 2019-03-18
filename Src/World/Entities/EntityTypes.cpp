#include "SpotLightEntity.hpp"
#include "EntranceEntity.hpp"
#include "PointLightEntity.hpp"

void InitEntityTypes()
{
	DefineEntityType(EntityType::Make<SpotLightEntity>("SpotLight", "Spot Light"));
	DefineEntityType(EntityType::Make<PointLightEntity>("PointLight", "Point Light"));
	DefineEntityType(EntityType::Make<EntranceEntity>("Entrance", "Entrance"));
}
