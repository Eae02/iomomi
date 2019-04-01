#pragma once

#include <btBulletDynamicsCommon.h>

namespace bullet
{
	extern btDefaultCollisionConfiguration* collisionConfig;
	extern btCollisionDispatcher* dispatcher;
	extern btSequentialImpulseConstraintSolver* solver;
	
	void Init();
	void Destroy();
	
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

