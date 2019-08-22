#include "ECRigidBody.hpp"

ECRigidBody::~ECRigidBody()
{
	if (m_physicsWorld != nullptr)
	{
		m_physicsWorld->removeRigidBody(&m_rigidBody.value());
	}
}

void ECRigidBody::Init(float mass, btCollisionShape& shape)
{
	btVector3 localInertia;
	shape.calculateLocalInertia(mass, localInertia);
	
	m_rigidBody = btRigidBody(mass, &m_motionState, &shape, localInertia);
}

void ECRigidBody::InitStatic(btCollisionShape& shape)
{
	m_rigidBody = btRigidBody(0.0f, &m_motionState, &shape);
}

void ECRigidBody::SetMass(float mass)
{
	btVector3 localInertia;
	m_rigidBody->getCollisionShape()->calculateLocalInertia(mass, localInertia);
	m_rigidBody->setMassProps(mass, localInertia);
}

void ECRigidBody::PullTransform(eg::Entity& entity, bool clearVelocity)
{
	ECRigidBody& rigidBody = entity.GetComponent<ECRigidBody>();
	
	btTransform transform;
	transform.setIdentity();
	
	if (eg::ECPosition3D* positionEC = entity.FindComponent<eg::ECPosition3D>())
	{
		transform.setOrigin(bullet::FromGLM(positionEC->position));
	}
	
	if (eg::ECRotation3D* rotationEC = entity.FindComponent<eg::ECRotation3D>())
	{
		transform.setRotation(bullet::FromGLM(rotationEC->rotation));
	}
	
	rigidBody.m_motionState.setWorldTransform(transform);
	rigidBody.m_rigidBody->setWorldTransform(transform);
	
	if (clearVelocity)
	{
		rigidBody.m_rigidBody->setLinearVelocity(btVector3(0.0f, 0.0f, 0.0f));
		rigidBody.m_rigidBody->setAngularVelocity(btVector3(0.0f, 0.0f, 0.0f));
		rigidBody.m_rigidBody->clearForces();
	}
}

void ECRigidBody::PushTransform(eg::Entity& entity)
{
	ECRigidBody& rigidBody = entity.GetComponent<ECRigidBody>();
	
	btTransform transform;
	rigidBody.m_motionState.getWorldTransform(transform);
	
	if (eg::ECPosition3D* positionEC = entity.FindComponent<eg::ECPosition3D>())
	{
		positionEC->position = bullet::ToGLM(transform.getOrigin());
	}
	
	if (eg::ECRotation3D* rotationEC = entity.FindComponent<eg::ECRotation3D>())
	{
		rotationEC->rotation = bullet::ToGLM(transform.getRotation());
	}
}
