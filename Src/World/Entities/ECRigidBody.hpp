#pragma once

#include "../BulletPhysics.hpp"

struct ECRigidBody
{
public:
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
	
	void Init(float mass, btCollisionShape& shape);
	
	//Copies the transform from the given entity's transform components to the rigid body
	static void PullTransform(eg::Entity& entity);
	
	//Copies the transform from the given entity's rigid body to its transform components
	static void PushTransform(eg::Entity& entity);
	
private:
	btDefaultMotionState m_motionState;
	std::optional<btRigidBody> m_rigidBody;
};
