#pragma once

#include "../Dir.hpp"

struct ForceFieldSetGravity
{
	Dir newGravity;
};

struct ECForceField
{
	glm::vec3 radius { 1.0f };
	
	Dir newGravity;
	
	void HandleMessage(eg::Entity& entity, const struct DrawMessage& message);
	void HandleMessage(eg::Entity& entity, const struct EditorDrawMessage& message);
	void HandleMessage(eg::Entity& entity, const struct EditorRenderImGuiMessage& message);
	
	static std::optional<Dir> CheckIntersection(const eg::EntityManager& entityManager, const eg::AABB& aabb);
	
	static eg::Entity* CreateEntity(eg::EntityManager& entityManager);
	
	static eg::EntitySignature EntitySignature;
	
	static eg::MessageReceiver MessageReceiver;
	
	static eg::IEntitySerializer* EntitySerializer;
};
