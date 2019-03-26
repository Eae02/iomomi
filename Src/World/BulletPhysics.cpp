#include "BulletPhysics.hpp"

namespace bullet
{
	btDefaultCollisionConfiguration* collisionConfig;
	btCollisionDispatcher* dispatcher;
	btSequentialImpulseConstraintSolver* solver;
	
	void Init()
	{
		collisionConfig = new btDefaultCollisionConfiguration();
		dispatcher = new btCollisionDispatcher(collisionConfig);
		solver = new btSequentialImpulseConstraintSolver();
	}
	
	void Destroy()
	{
		delete solver;
		delete dispatcher;
		delete collisionConfig;
	}
}
