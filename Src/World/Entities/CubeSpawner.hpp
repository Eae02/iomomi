#pragma once

struct WorldUpdateArgs;

namespace CubeSpawner
{
	extern eg::EntitySignature EntitySignature;
	
	eg::Entity* CreateEntity(eg::EntityManager& entityManager);
	
	void Update(const WorldUpdateArgs& updateArgs);
	
	extern eg::IEntitySerializer* EntitySerializer;
}
