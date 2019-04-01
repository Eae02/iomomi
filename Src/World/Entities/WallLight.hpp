#pragma once

namespace WallLight
{
	eg::Entity* CreateEntity(eg::EntityManager& entityManager);
	
	extern eg::IEntitySerializer* EntitySerializer;
}
