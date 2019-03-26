#include "BulletPhysics.hpp"

namespace bullet
{
	btDefaultCollisionConfiguration* collisionConfig;
	btCollisionDispatcher* dispatcher;
	btBroadphaseInterface* broadphase;
	btSequentialImpulseConstraintSolver* solver;
	
	void Init()
	{
		collisionConfig = new btDefaultCollisionConfiguration();
		dispatcher = new btCollisionDispatcher(collisionConfig);
		broadphase = new btDbvtBroadphase();
		solver = new btSequentialImpulseConstraintSolver();
		
	}
	
	void Destroy()
	{
		delete solver;
		delete broadphase;
		delete dispatcher;
		delete collisionConfig;
	}
}
