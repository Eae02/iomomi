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
	
	void AddCollisionMesh(btTriangleMesh& targetMesh, const eg::CollisionMesh& collisionMesh)
	{
		btIndexedMesh mesh;
		mesh.m_indexType = PHY_INTEGER;
		mesh.m_vertexType = PHY_FLOAT;
		
		mesh.m_numVertices = collisionMesh.NumVertices();
		mesh.m_vertexStride = sizeof(float) * 4;
		mesh.m_vertexBase = reinterpret_cast<const uint8_t*>(collisionMesh.Vertices());
		
		mesh.m_numTriangles = (int)collisionMesh.NumIndices() / 3;
		mesh.m_triangleIndexStride = sizeof(uint32_t) * 3;
		mesh.m_triangleIndexBase = reinterpret_cast<const uint8_t*>(collisionMesh.Indices());
		
		targetMesh.addIndexedMesh(mesh, PHY_INTEGER);
	}
}
