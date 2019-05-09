#pragma once

#include "ECEditorVisible.hpp"
#include "Messages.hpp"

struct ECPlatform
{
	void HandleMessage(eg::Entity& entity, const EditorDrawMessage& message);
	void HandleMessage(eg::Entity& entity, const EditorRenderImGuiMessage& message);
	void HandleMessage(eg::Entity& entity, const DrawMessage& message);
	void HandleMessage(eg::Entity& entity, const RayIntersectMessage& message);
	void HandleMessage(eg::Entity& entity, const CalculateCollisionMessage& message);
	
	static glm::vec3 GetPosition(const eg::Entity& entity);
	
	static eg::Entity* FindPlatform(const eg::AABB& searchAABB, eg::EntityManager& entityManager);
	
	static eg::AABB GetAABB(const eg::Entity& entity);
	
	static void Update(const struct WorldUpdateArgs& args);
	
	static eg::Entity* CreateEntity(eg::EntityManager& entityManager);
	
	static eg::EntitySignature EntitySignature;
	static eg::IEntitySerializer* EntitySerializer;
	
	static eg::MessageReceiver MessageReceiver;
	
	float slideProgress = 0.0f;
	float slideTime = 1.0f;
	glm::vec2 slideOffset;
	glm::vec3 moveDelta;
};
