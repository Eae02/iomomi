#include "SpotLightEntity.hpp"

void InitEntityTypes()
{
	DefineEntityType(EntityType::Make<SpotLightEntity>("SpotLight", "Spot Light"));
}
