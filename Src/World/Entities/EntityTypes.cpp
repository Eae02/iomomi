#include "SpotLightEntity.hpp"
#include "EntranceEntity.hpp"

void InitEntityTypes()
{
	DefineEntityType(EntityType::Make<SpotLightEntity>("SpotLight", "Spot Light"));
	DefineEntityType(EntityType::Make<EntranceEntity>("Entrance", "Entrance"));
}
