#pragma once

#include "../BulletPhysics.hpp"

struct ECRigidBody
{
public:
	friend class World;
	
	~ECRigidBody();
	
	ECRigidBody() = default;
	ECRigidBody(const ECRigidBody& other) = delete;
	ECRigidBody(ECRigidBody&& other) = delete;
	ECRigidBody& operator=(const ECRigidBody& other) = delete;
	ECRigidBody& operator=(ECRigidBody&& other) = delete;
	
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
	
	//Copies the transform from the given entity's transform components to the rigid body
	static void PullTransform(eg::Entity& entity, bool clearVelocity = true);
	
	//Copies the transform from the given entity's rigid body to its transform components
	static void PushTransform(eg::Entity& entity);
	
private:
	btDefaultMotionState m_motionState;
	std::optional<btRigidBody> m_rigidBody;
	btDiscreteDynamicsWorld* m_physicsWorld = nullptr;
};
