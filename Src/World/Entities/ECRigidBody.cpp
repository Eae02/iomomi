#include "ECRigidBody.hpp"

void ECRigidBody::Init(float mass, btCollisionShape& shape)
{
	btVector3 localInertia;
	shape.calculateLocalInertia(mass, localInertia);
	
	m_rigidBody = btRigidBody(mass, &m_motionState, &shape, localInertia);
}

void ECRigidBody::PullTransform(eg::Entity& entity)
{
	ECRigidBody* rigidBody = entity.GetComponent<ECRigidBody>();
	
	btTransform transform;
	transform.setIdentity();
	
	if (eg::ECPosition3D* positionEC = entity.GetComponent<eg::ECPosition3D>())
	{
		transform.setOrigin(bullet::FromGLM(positionEC->position));
	}
	
	if (eg::ECRotation3D* rotationEC = entity.GetComponent<eg::ECRotation3D>())
	{
		transform.setRotation(bullet::FromGLM(rotationEC->rotation));
	}
	
	rigidBody->m_motionState.setWorldTransform(transform);
	rigidBody->m_rigidBody->setWorldTransform(transform);
	
	rigidBody->m_rigidBody->setLinearVelocity(btVector3(0.0f, 0.0f, 0.0f));
	rigidBody->m_rigidBody->setAngularVelocity(btVector3(0.0f, 0.0f, 0.0f));
	rigidBody->m_rigidBody->clearForces();
}

void ECRigidBody::PushTransform(eg::Entity& entity)
{
	
}
