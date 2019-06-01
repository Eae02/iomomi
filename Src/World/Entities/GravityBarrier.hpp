#pragma once

#include "../Dir.hpp"

class ECGravityBarrier
{
public:
	static eg::EntitySignature EntitySignature;
	
	static eg::IEntitySerializer* EntitySerializer;
	
	static eg::MessageReceiver MessageReceiver;
	
	static eg::Entity* CreateEntity(eg::EntityManager& entityManager);
	
	void HandleMessage(eg::Entity& entity, const struct DrawMessage& message);
	void HandleMessage(eg::Entity& entity, const struct EditorDrawMessage& message);
	void HandleMessage(eg::Entity& entity, const struct EditorRenderImGuiMessage& message);
	void HandleMessage(eg::Entity& entity, const struct CalculateCollisionMessage& message);
	
	int flowDirection = 0;
	int upPlane = 0;
	glm::vec2 size { 2.0f };
	
private:
	std::tuple<glm::vec3, glm::vec3> GetTangents() const;
	
	void Draw(eg::Entity& entity, eg::MeshBatchOrdered& meshBatch) const;
};
