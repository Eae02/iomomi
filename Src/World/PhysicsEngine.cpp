#include <netinet/in.h>
#include "PhysicsEngine.hpp"
#include "Collision.hpp"
#include "../Editor/PrimitiveRenderer.hpp"
#include "../Graphics/PhysicsDebugRenderer.hpp"
#include "../Graphics/GraphicsCommon.hpp"

static float* gravityMag = eg::TweakVarFloat("gravity", 10, 0);
static float* dispLockDist = eg::TweakVarFloat("phys_dlock_dist", 0.01f, 0);
static float* dispLockTime = eg::TweakVarFloat("phys_dlock_time", 0.1f, 0);

PhysicsObject* PhysicsEngine::FindFloorObject(PhysicsObject& object, const glm::vec3& down) const
{
	CheckCollisionResult colResult = CheckForCollision(object, object.position + down * 0.1f, object.rotation);
	
	if (colResult.collided)
	{
		return colResult.other;
	}
	return nullptr;
}

void PhysicsEngine::CopyParentMove(PhysicsObject& object, float dt)
{
	if (object.hasCopiedParentMove || !object.canBePushed || !object.canCarry || glm::length2(object.gravity) < 1E-3f)
		return;
	object.hasCopiedParentMove = true;
	
	PhysicsObject* floor = FindFloorObject(object, object.gravity);
	if (floor && floor->canCarry)
	{
		floor->childObjects.push_back(&object);
		CopyParentMove(*floor, dt);
		
		object.collisionDepth = 10;
		ApplyMovement(*floor, dt);
		object.move += floor->actualMove;
		object.collisionDepth = 0;
	}
}

void PhysicsEngine::BeginCollect()
{
	m_objects.clear();
}

void PhysicsEngine::Simulate(float dt)
{
	for (PhysicsObject* object : m_objects)
	{
		object->velocity += object->gravity * dt * *gravityMag;
		object->velocity += object->force * dt;
		object->move += object->velocity * dt;
		object->collisionDepth = 0;
		object->hasCopiedParentMove = false;
		object->childObjects.clear();
		object->pushForce = { };
		object->force = { };
	}
	
	for (PhysicsObject* object : m_objects)
	{
		CopyParentMove(*object, dt);
	}
	
	for (PhysicsObject* object : m_objects)
	{
		ApplyMovement(*object, dt);
	}
	
	for (PhysicsObject* object : m_objects)
	{
		object->displayPosition = object->position;
		
		if (object->firstFrame || glm::distance2(object->lockedDisplayPosition, object->position) > *dispLockDist * *dispLockDist)
		{
			object->timeUntilLockDisplayPosition = *dispLockTime;
			object->lockedDisplayPosition = object->position;
			object->firstFrame = false;
		}
		else if (object->timeUntilLockDisplayPosition < 0)
		{
			object->displayPosition = object->lockedDisplayPosition;
		}
		else
		{
			object->timeUntilLockDisplayPosition -= dt;
			if (object->timeUntilLockDisplayPosition < 0)
			{
				object->displayPosition = object->lockedDisplayPosition = object->position;
			}
		}
		
		object->move = { };
	}
}

void PhysicsEngine::EndCollect()
{
	for (PhysicsObject* object : m_objects)
	{
		object->hasCopiedParentMove = false;
		object->childObjects.clear();
		object->actualMove = { };
		object->didMove = false;
	}
}

void PhysicsEngine::RegisterObject(PhysicsObject* object)
{
	m_objects.push_back(object);
	object->needsFlippedWinding = glm::determinant(glm::mat3_cast(object->rotation)) < 0;
}

void PhysicsEngine::ApplyMovement(PhysicsObject& object, float dt, const CollisionCallback& callback)
{
	if (object.collisionDepth > 2)
	{
		object.move = { };
		return;
	}
	object.collisionDepth++;
	
	if (object.constrainMove)
	{
		object.move = object.constrainMove(object, object.move);
	}
	
	bool didCollide = true;
	
	constexpr float MAX_MOVE_LEN = 10;
	constexpr float MAX_MOVE_PER_STEP = 0.5f;
	constexpr int MAX_ITERATIONS = 10;
	for (int i = 0; i < MAX_ITERATIONS && didCollide; i++)
	{
		float moveLen = glm::length(object.move);
		if (moveLen < 1E-4f)
			break;
		
		if (moveLen > MAX_MOVE_LEN)
		{
			moveLen = MAX_MOVE_LEN;
			object.move *= MAX_MOVE_LEN / moveLen;
		}
		
		didCollide = false;
		int numMoveIterations = std::ceil(moveLen / MAX_MOVE_PER_STEP);
		for (int s = 1; s <= numMoveIterations; s++)
		{
			glm::vec3 partialMove = object.move * ((float)s / (float)numMoveIterations);
			glm::vec3 newPos = object.position + partialMove;
			CheckCollisionResult colRes = CheckForCollision(object, newPos, {});
			if (colRes.collided)
			{
				if (callback)
				{
					callback(*colRes.other, colRes.correction);
				}
				colRes.other->pushForce += -colRes.correction;
				if (colRes.other->canBePushed)
				{
					ApplyMovement(*colRes.other, dt);
					colRes.other->move += -colRes.correction * 1.05f;
					ApplyMovement(*colRes.other, dt);
					colRes = CheckForCollision(object, newPos, { });
					if (!colRes.collided)
						continue;
				}
				object.velocity -= colRes.correction * (glm::dot(colRes.correction, object.velocity) / glm::length2(colRes.correction));
				object.move = partialMove + colRes.correction;
				
				glm::vec3 relativeVel = object.velocity - colRes.other->velocity;
				if (glm::length2(relativeVel) > 1E-6f)
				{
					float friction = object.friction * colRes.other->friction;
					float velFactor = 1.0f - (friction * glm::length(colRes.correction)) / (dt * glm::length(relativeVel));
					object.velocity = relativeVel * std::max(velFactor, 0.0f) + colRes.other->velocity;
				}
				
				didCollide = true;
				break;
			}
		}
	}
	
	if (!didCollide && glm::length2(object.move) > 1E-6f)
	{
		object.position += object.move;
		object.actualMove += object.move;
		object.didMove = true;
	}
	
	object.move = { };
	
	object.collisionDepth--;
}

bool PhysicsEngine::CheckCollisionCallbacks(const PhysicsObject& a, const PhysicsObject& b)
{
	if (a.shouldCollide != nullptr && !a.shouldCollide(a, b))
		return false;
	if (b.shouldCollide != nullptr && !b.shouldCollide(b, a))
		return false;
	return true;
}

PhysicsObject* PhysicsEngine::CheckForCollision(CollisionResponseCombiner& combiner, const PhysicsObject& currentObject,
	const eg::AABB& shape, const glm::vec3& position, const glm::quat& rotation) const
{
	eg::AABB shiftedAABB(shape.min + position, shape.max + position);
	
	PhysicsObject* otherObject = nullptr;
	
	for (PhysicsObject* object : m_objects)
	{
		if (object == &currentObject || !CheckCollisionCallbacks(currentObject, *object))
			continue;
		
		std::optional<glm::vec3> correction;
		
		if (const eg::CollisionMesh* const* mesh = std::get_if<const eg::CollisionMesh*>(&object->shape))
		{
			glm::mat4 meshTransform = glm::translate(glm::mat4(1.0f), object->position) * glm::mat4_cast(object->rotation);
			correction = CheckCollisionAABBTriangleMesh(shiftedAABB, currentObject.move, **mesh, meshTransform,
				object->needsFlippedWinding);
		}
		else if (const eg::AABB* otherAABB = std::get_if<eg::AABB>(&object->shape))
		{
			OrientedBox otherOBB;
			otherOBB.center = object->rotation * otherAABB->Center() + object->position;
			otherOBB.radius = otherAABB->Size() / 2.0f;
			otherOBB.rotation = object->rotation;
			correction = CheckCollisionAABBOrientedBox(shiftedAABB, otherOBB, currentObject.move);
		}
		
		if (combiner.Update(correction))
		{
			otherObject = object;
		}
	}
	
	return otherObject;
}

PhysicsObject* PhysicsEngine::CheckForCollision(CollisionResponseCombiner& combiner, const PhysicsObject& currentObject,
	const eg::CollisionMesh* shape, const glm::vec3& position, const glm::quat& rotation) const
{
	//Collision mesh object cannot currently collide with other things,
	// but this is ok for now since there are no such objects that move.
	return nullptr;
}

PhysicsEngine::CheckCollisionResult PhysicsEngine::CheckForCollision(const PhysicsObject& currentObject,
	const glm::vec3& position, const glm::quat& rotation) const
{
	CollisionResponseCombiner combiner;
	
	PhysicsObject* otherObject = nullptr;
	std::visit([&] (const auto& shape)
	{
		otherObject = CheckForCollision(combiner, currentObject, shape, position, rotation);
	}, currentObject.shape);
	
	CheckCollisionResult result;
	if (combiner.GetCorrection())
	{
		result.collided = true;
		result.other = otherObject;
		result.correction = *combiner.GetCorrection();
	}
	return result;
}

static inline std::optional<float> RayIntersect(
	const eg::Ray& ray, const glm::vec3& objPosition, const glm::quat& objRotation, const eg::AABB& shape)
{
	OrientedBox obb;
	obb.center = objPosition + shape.Center();
	obb.radius = shape.Size() / 2.0f;
	obb.rotation = objRotation;
	return RayIntersectOrientedBox(ray, obb);
}

static inline std::optional<float> RayIntersect(
	const eg::Ray& ray, const glm::vec3& objPosition, const glm::quat& objRotation, const eg::CollisionMesh* shape)
{
	glm::quat invRotation = glm::inverse(objRotation);
	glm::vec3 localStart = invRotation * (ray.GetStart() - objPosition);
	glm::vec3 localDir = invRotation * ray.GetDirection();
	eg::Ray localRay(localStart, localDir);
	float intersectPos;
	if (shape->Intersect(localRay, intersectPos) != -1)
	{
		return intersectPos;
	}
	return {};
}

std::pair<PhysicsObject*, float> PhysicsEngine::RayIntersect(const eg::Ray& ray, uint32_t mask) const
{
	float minIntersect = INFINITY;
	PhysicsObject* intersectedObject = nullptr;
	for (PhysicsObject* object : m_objects)
	{
		if (!(object->rayIntersectMask & mask))
			continue;
		std::visit([&] (const auto& shape)
		{
			if (std::optional<float> intersect = ::RayIntersect(ray, object->position, object->rotation, shape))
			{
				if (*intersect < minIntersect)
				{
					minIntersect = *intersect;
					intersectedObject = object;
				}
			}
		}, object->shape);
	}
	return { intersectedObject, minIntersect };
}

void GetDebugRenderDataForShape(PhysicsDebugRenderData& dataOut, const PhysicsObject& obj,
	const eg::CollisionMesh* mesh, uint32_t color)
{
	for (size_t i = 0; i < mesh->NumIndices(); i++)
	{
		dataOut.triangleIndices.push_back(mesh->Indices()[i] + dataOut.vertices.size());
	}
	
	for (size_t v = 0; v < mesh->NumVertices(); v++)
	{
		glm::vec3 transformedPos = obj.position + (obj.rotation * mesh->Vertex(v));
		PhysicsDebugVertex& vertex = dataOut.vertices.emplace_back();
		vertex.x = transformedPos.x;
		vertex.y = transformedPos.y;
		vertex.z = transformedPos.z;
		vertex.color = color;
	}
}

void GetDebugRenderDataForShape(PhysicsDebugRenderData& dataOut, const PhysicsObject& obj,
	const eg::AABB& aabb, uint32_t color)
{
	for (const std::pair<uint32_t, uint32_t>& edge : cubeMesh::edges)
	{
		dataOut.lineIndices.push_back(dataOut.vertices.size() + edge.first);
		dataOut.lineIndices.push_back(dataOut.vertices.size() + edge.second);
	}
	
	const glm::vec3 aabbCenter = aabb.Center();
	const glm::vec3 aabbDiag = aabb.Size() / 2.0f;
	
	for (const glm::ivec3& localPos : cubeMesh::vertices)
	{
		glm::vec3 worldPos = obj.rotation * (aabbCenter + aabbDiag * glm::vec3(localPos)) + obj.position;
		PhysicsDebugVertex& vertex = dataOut.vertices.emplace_back();
		vertex.x = worldPos.x;
		vertex.y = worldPos.y;
		vertex.z = worldPos.z;
		vertex.color = color;
	}
}

void PhysicsEngine::GetDebugRenderData(PhysicsDebugRenderData& dataOut) const
{
	for (const PhysicsObject* object : m_objects)
	{
		uint32_t color = htonl((object->debugColor << 8) | 0xFF);
		
		std::visit([&] (const auto& shape)
		{
			GetDebugRenderDataForShape(dataOut, *object, shape, color);
		}, object->shape);
	}
}