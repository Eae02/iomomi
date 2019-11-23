#pragma once

#include "../../BulletPhysics.hpp"

struct RigidBodyComp
{
public:
	friend class World;
	
	~RigidBodyComp();
	
	RigidBodyComp() = default;
	RigidBodyComp(const RigidBodyComp& other) = delete;
	RigidBodyComp(RigidBodyComp&& other) = delete;
	RigidBodyComp& operator=(const RigidBodyComp& other) = delete;
	RigidBodyComp& operator=(RigidBodyComp&& other) = delete;
	
	const btRigidBody* GetRigidBody() const
	{
		return m_rigidBody.has_value() ? &m_rigidBody.value() : nullptr;
	}
	
	btRigidBody* GetRigidBody()
	{
		return m_rigidBody.has_value() ? &m_rigidBody.value() : nullptr;
	}
	
	btTransform GetBulletTransform() const
	{
		btTransform transform;
		m_motionState.getWorldTransform(transform);
		return transform;
	}
	
	void SetWorldTransform(const btTransform& transform)
	{
		m_motionState.setWorldTransform(transform);
		m_rigidBody->setWorldTransform(transform);
	}
	
	void Init(float mass, btCollisionShape& shape);
	void InitStatic(btCollisionShape& shape);
	
	void SetMass(float mass);
	
	void SetTransform(const glm::vec3& pos, const glm::quat& rot, bool clearVelocity = true);
	
	std::pair<glm::vec3, glm::quat> GetTransform() const;
	
private:
	btDefaultMotionState m_motionState;
	std::optional<btRigidBody> m_rigidBody;
	btDiscreteDynamicsWorld* m_physicsWorld = nullptr;
};
