#pragma once

struct WorldUpdateArgs;

namespace Cube
{
	constexpr float RADIUS = 0.4f;
	
	extern eg::EntitySignature EntitySignature;
	
	eg::Entity* CreateEntity(eg::EntityManager& entityManager);
	
	eg::Entity* Spawn(eg::EntityManager& entityManager, const glm::vec3& position, bool canFloat);
	
	void UpdatePreSim(const WorldUpdateArgs& args);
	void UpdatePostSim(const WorldUpdateArgs& args);
	
	extern eg::IEntitySerializer* EntitySerializer;
}
