#include "RigidBodyComp.hpp"

RigidBodyComp::~RigidBodyComp()
{
	if (std::shared_ptr<btDiscreteDynamicsWorld> physicsWorld = m_physicsWorld.lock())
	{
		physicsWorld->removeRigidBody(&m_rigidBody.value());
	}
}

void RigidBodyComp::Init(float mass, btCollisionShape& shape)
{
	btVector3 localInertia;
	shape.calculateLocalInertia(mass, localInertia);
	
	m_rigidBody = btRigidBody(mass, &m_motionState, &shape, localInertia);
}

void RigidBodyComp::InitStatic(btCollisionShape& shape)
{
	m_rigidBody = btRigidBody(0.0f, &m_motionState, &shape);
}

void RigidBodyComp::SetMass(float mass)
{
	btVector3 localInertia;
	m_rigidBody->getCollisionShape()->calculateLocalInertia(mass, localInertia);
	m_rigidBody->setMassProps(mass, localInertia);
}

void RigidBodyComp::SetTransform(const glm::vec3& pos, const glm::quat& rot, bool clearVelocity)
{
	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(bullet::FromGLM(pos));
	transform.setRotation(bullet::FromGLM(rot));
	
	m_motionState.setWorldTransform(transform);
	m_rigidBody->setWorldTransform(transform);
	
	if (clearVelocity)
	{
		m_rigidBody->setLinearVelocity(btVector3(0.0f, 0.0f, 0.0f));
		m_rigidBody->setAngularVelocity(btVector3(0.0f, 0.0f, 0.0f));
		m_rigidBody->clearForces();
	}
}

std::pair<glm::vec3, glm::quat> RigidBodyComp::GetTransform() const
{
	btTransform transform;
	m_motionState.getWorldTransform(transform);
	
	return std::make_pair(bullet::ToGLM(transform.getOrigin()), bullet::ToGLM(transform.getRotation()));
}
