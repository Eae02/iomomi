#pragma once

#include "../../BulletPhysics.hpp"

#include <list>

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
		return !m_rigidBodies.empty() ? &m_rigidBodies.front() : nullptr;
	}
	
	btRigidBody* GetRigidBody()
	{
		return !m_rigidBodies.empty() ? &m_rigidBodies.front() : nullptr;
	}
	
	btTransform GetBulletTransform() const
	{
		btTransform transform;
		m_motionStates.front().getWorldTransform(transform);
		return transform;
	}
	
	void SetWorldTransform(const btTransform& transform);
	
	btRigidBody& Init(class Ent* entity, float mass, btCollisionShape& shape);
	btRigidBody& InitStatic(class Ent* entity, btCollisionShape& shape);
	
	void SetMass(float mass);
	
	void SetTransform(const glm::vec3& pos, const glm::quat& rot, bool clearVelocity = true);
	
	std::pair<glm::vec3, glm::quat> GetTransform() const;
	
private:
	std::list<btDefaultMotionState> m_motionStates;
	std::list<btRigidBody> m_rigidBodies;
	
	bool m_worldAssigned = false;
	std::weak_ptr<btDiscreteDynamicsWorld> m_physicsWorld;
};
