#pragma once

namespace GooPlane
{
	extern eg::EntitySignature EntitySignature;
	
	extern eg::IEntitySerializer* EntitySerializer;
	
	eg::Entity* CreateEntity(eg::EntityManager& entityManager);
}
