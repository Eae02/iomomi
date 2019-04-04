#pragma once

#include "ECWallMounted.hpp"

struct ECFloorButton
{
	static eg::Entity* CreateEntity(eg::EntityManager& entityManager);
	
	static constexpr float SCALE = 1.0f;
	
	static eg::AABB GetAABB(const eg::Entity& entity)
	{
		return ECWallMounted::GetAABB(entity, SCALE, 0.2f);
	}
	
	void HandleMessage(eg::Entity& entity, const struct EditorDrawMessage& message);
	void HandleMessage(eg::Entity& entity, const struct EditorRenderImGuiMessage& message);
	void HandleMessage(eg::Entity& entity, const struct DrawMessage& message);
	
	static eg::IEntitySerializer* EntitySerializer;
	
	static eg::EntitySignature EntitySignature;
	
	static eg::MessageReceiver MessageReceiver;
};
