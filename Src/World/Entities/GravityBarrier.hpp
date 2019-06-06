#pragma once

#include "../Dir.hpp"
#include "../BulletPhysics.hpp"

class ECGravityBarrier
{
public:
	static eg::EntitySignature EntitySignature;
	
	static eg::IEntitySerializer* EntitySerializer;
	
	static eg::MessageReceiver MessageReceiver;
	
	static eg::Entity* CreateEntity(eg::EntityManager& entityManager);
	
	ECGravityBarrier()
		: m_collisionShape({}) { }
	
	void HandleMessage(eg::Entity& entity, const struct DrawMessage& message);
	void HandleMessage(eg::Entity& entity, const struct EditorDrawMessage& message);
	void HandleMessage(eg::Entity& entity, const struct EditorRenderImGuiMessage& message);
	void HandleMessage(eg::Entity& entity, const struct CalculateCollisionMessage& message);
	
	std::vector<glm::vec3> GetActivationPoints(const eg::Entity& entity) const;
	
	int BlockedAxis() const;
	
	static void Update(eg::EntityManager& entityManager, float dt);
	
	static glm::mat4 GetTransform(const eg::Entity& entity);
	
	enum class ActivateAction
	{
		Disable,
		Enable,
		Rotate
	};
	
	int flowDirection = 0;
	int upPlane = 0;
	ActivateAction activateAction = ActivateAction::Disable;
	glm::vec2 size { 2.0f };
	
private:
	friend struct GravityBarrierSerializer;
	
	std::tuple<glm::vec3, glm::vec3> GetTangents() const;
	
	void Draw(eg::Entity& entity, eg::MeshBatchOrdered& meshBatch) const;
	
	float m_opacity = 1;
	bool m_enabled = true;
	int m_flowDirectionOffset = 0;
	btBoxShape m_collisionShape;
};
