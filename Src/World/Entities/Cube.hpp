#pragma once

struct WorldUpdateArgs;

namespace Cube
{
	extern eg::EntitySignature EntitySignature;
	
	eg::Entity* CreateEntity(eg::EntityManager& entityManager);
	
	void UpdatePreSim(const WorldUpdateArgs& args);
	void UpdatePostSim(const WorldUpdateArgs& args);
	
	extern eg::IEntitySerializer* EntitySerializer;
}
