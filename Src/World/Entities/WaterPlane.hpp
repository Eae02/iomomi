#pragma once

struct ECWaterPlane { };

namespace WaterPlane
{
	extern eg::EntitySignature EntitySignature;
	
	extern eg::IEntitySerializer* EntitySerializer;
	
	eg::Entity* CreateEntity(eg::EntityManager& entityManager);
}
