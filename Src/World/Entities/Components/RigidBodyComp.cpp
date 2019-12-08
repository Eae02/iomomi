#include "RigidBodyComp.hpp"

RigidBodyComp::~RigidBodyComp()
{
	if (std::shared_ptr<btDiscreteDynamicsWorld> physicsWorld = m_physicsWorld.lock())
	{
		for (btRigidBody& body : m_rigidBodies)
		{
			physicsWorld->removeRigidBody(&body);
		}
	}
}

btRigidBody& RigidBodyComp::Init(class Ent* entity, float mass, btCollisionShape& shape)
{
	EG_ASSERT(!m_worldAssigned);
	
	btVector3 localInertia;
	shape.calculateLocalInertia(mass, localInertia);
	
	btRigidBody& body = m_rigidBodies.emplace_back(mass, &m_motionStates.emplace_back(), &shape, localInertia);
	body.setUserPointer(entity);
	return body;
}

btRigidBody& RigidBodyComp::InitStatic(class Ent* entity, btCollisionShape& shape)
{
	EG_ASSERT(!m_worldAssigned);
	btRigidBody& body = m_rigidBodies.emplace_back(0.0f, &m_motionStates.emplace_back(), &shape);
	body.setUserPointer(entity);
	return body;
}

void RigidBodyComp::SetMass(float mass)
{
	for (btRigidBody& body : m_rigidBodies)
	{
		btVector3 localInertia;
		body.getCollisionShape()->calculateLocalInertia(mass, localInertia);
		body.setMassProps(mass, localInertia);
	}
}

void RigidBodyComp::SetWorldTransform(const btTransform& transform)
{
	for (btRigidBody& body : m_rigidBodies)
	{
		body.getMotionState()->setWorldTransform(transform);
		body.setWorldTransform(transform);
	}
}

void RigidBodyComp::SetTransform(const glm::vec3& pos, const glm::quat& rot, bool clearVelocity)
{
	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(bullet::FromGLM(pos));
	transform.setRotation(bullet::FromGLM(rot));
	
	for (btRigidBody& body : m_rigidBodies)
	{
		body.getMotionState()->setWorldTransform(transform);
		body.setWorldTransform(transform);
		
		if (clearVelocity)
		{
			body.setLinearVelocity(btVector3(0.0f, 0.0f, 0.0f));
			body.setAngularVelocity(btVector3(0.0f, 0.0f, 0.0f));
			body.clearForces();
		}
	}
}

std::pair<glm::vec3, glm::quat> RigidBodyComp::GetTransform() const
{
	btTransform transform;
	m_motionStates.front().getWorldTransform(transform);
	
	return std::make_pair(bullet::ToGLM(transform.getOrigin()), bullet::ToGLM(transform.getRotation()));
}
