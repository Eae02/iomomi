#pragma once

#include "../Dir.hpp"

struct ForceFieldSetGravity
{
	Dir newGravity;
};

struct ForceFieldParticle
{
	glm::vec3 start;
	float elapsedTime;
};

struct ECForceField
{
public:
	glm::vec3 radius { 1.0f };
	
	Dir newGravity;
	
	void HandleMessage(eg::Entity& entity, const struct DrawMessage& message);
	void HandleMessage(eg::Entity& entity, const struct EditorDrawMessage& message);
	void HandleMessage(eg::Entity& entity, const struct EditorRenderImGuiMessage& message);
	
	static std::optional<Dir> CheckIntersection(eg::EntityManager& entityManager, const eg::AABB& aabb);
	
	static void Update(float dt, eg::EntityManager& entityManager);
	
	static eg::Entity* CreateEntity(eg::EntityManager& entityManager);
	
	static eg::EntitySignature EntitySignature;
	
	static eg::MessageReceiver MessageReceiver;
	
	static eg::IEntitySerializer* EntitySerializer;
	
private:
	std::vector<ForceFieldParticle> particles;
	float timeSinceEmission = 0;
};
