#pragma once

#include <btBulletDynamicsCommon.h>

namespace bullet
{
	extern btDefaultCollisionConfiguration* collisionConfig;
	extern btCollisionDispatcher* dispatcher;
	extern btSequentialImpulseConstraintSolver* solver;
	
	constexpr int PICKED_OBJECT_COLLISION_MASK = 64;
	constexpr int BASIC_COLLISION_FILTER = 63 | PICKED_OBJECT_COLLISION_MASK;
	constexpr int GRAVITY_BARRIER_COLLISION_MASK0 = 128;
	
	constexpr float GRAVITY = 10;
	
	void Init();
	void Destroy();
	
	void AddCollisionMesh(btTriangleMesh& targetMesh, const eg::CollisionMesh& collisionMesh);
	
	inline glm::vec3 ToGLM(const btVector3& v3)
	{
		return glm::vec3(v3.x(), v3.y(), v3.z());
	}
	
	inline glm::quat ToGLM(const btQuaternion& q)
	{
		return glm::quat(q.w(), q.x(), q.y(), q.z());
	}
	
	inline btVector3 FromGLM(const glm::vec3& v3)
	{
		return btVector3(v3.x, v3.y, v3.z);
	}
	
	inline btQuaternion FromGLM(const glm::quat& q)
	{
		return btQuaternion(q.x, q.y, q.z, q.w);
	}
}

