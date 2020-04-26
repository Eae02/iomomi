#include "PhysicsEngine.hpp"
#include "Collision.hpp"

static float* gravityMag = eg::TweakVarFloat("gravity", 10, 0);
static float* roundPrecision = eg::TweakVarFloat("phys_round_prec", 0.0005f, 0);

void PhysicsEngine::CopyParentMove(PhysicsObject& object)
{
	if (object.hasCopiedParentMove || !object.canBePushed || !object.canCarry || glm::length2(object.gravity) < 1E-3f)
		return;
	object.hasCopiedParentMove = true;
	
	CheckCollisionResult colResult = 
		CheckForCollision(object, object.position + object.gravity * 0.1f, object.rotation);
	
	if (colResult.collided && colResult.other->canCarry)
	{
		colResult.other->childObjects.push_back(&object);
		CopyParentMove(*colResult.other);
		object.move += colResult.other->move;
	}
}

void PhysicsEngine::Simulate(float dt)
{
	for (PhysicsObject* object : m_objects)
	{
		object->velocity += object->gravity * dt * *gravityMag;
		object->velocity += object->force * dt;
		object->move += object->velocity * dt;
		object->previousPosition = object->position;
		object->collisionDepth = 0;
		object->hasCopiedParentMove = false;
		object->childObjects.clear();
		object->pushForce = { };
	}
	
	for (PhysicsObject* object : m_objects)
		CopyParentMove(*object);
	
	for (PhysicsObject* object : m_objects)
	{
		ApplyMovement(*object);
	}
	
	for (PhysicsObject* object : m_objects)
	{
		object->displayPosition = glm::round(object->position / *roundPrecision) * *roundPrecision;
		object->move = { };
		object->force = { };
	}
	m_objects.clear();
}

void PhysicsEngine::RegisterObject(PhysicsObject* object)
{
	m_objects.push_back(object);
}

void PhysicsEngine::ApplyMovement(PhysicsObject& object)
{
	if (object.collisionDepth > 2)
	{
		object.move = { };
		return;
	}
	object.collisionDepth++;
	
	constexpr float MAX_MOVE_LEN = 10;
	constexpr float MAX_MOVE_PER_STEP = 0.5f;
	constexpr int MAX_ITERATIONS = 4;
	for (int i = 0; i < MAX_ITERATIONS; i++)
	{
		float moveLen = glm::length(object.move);
		if (moveLen < 1E-4f)
			break;
		
		if (moveLen > MAX_MOVE_LEN)
		{
			moveLen = MAX_MOVE_LEN;
			object.move *= MAX_MOVE_LEN / moveLen;
		}
		
		bool didCollide = false;
		int numMoveIterations = std::ceil(moveLen / MAX_MOVE_PER_STEP);
		for (int s = 1; s <= numMoveIterations; s++)
		{
			glm::vec3 partialMove = object.move * ((float)s / (float)numMoveIterations);
			glm::vec3 newPos = object.position + partialMove;
			CheckCollisionResult colRes = CheckForCollision(object, newPos, {});
			if (colRes.collided)
			{
				colRes.other->pushForce += -colRes.correction;
				if (colRes.other->canBePushed)
				{
					ApplyMovement(*colRes.other);
					colRes.other->move += -colRes.correction;
					ApplyMovement(*colRes.other);
					colRes = CheckForCollision(object, newPos, {});
					if (!colRes.collided)
						continue;
				}
				
				object.velocity -= colRes.correction * (glm::dot(colRes.correction, object.velocity) / glm::length2(colRes.correction));
				object.move = partialMove + colRes.correction;
				didCollide = true;
				break;
			}
		}
		object.position += object.move;
		object.move = { };
		if (!didCollide)
			break;
	}
	
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

PhysicsEngine::CheckCollisionResult PhysicsEngine::CheckForCollision(const PhysicsObject& currentObject,
	const eg::AABB& shape, const glm::vec3& position, const glm::quat& rotation)
{
	eg::AABB shiftedAABB(shape.min + position, shape.max + position);
	
	CollisionResponseCombiner combiner;
	PhysicsObject* otherObject = nullptr;
	
	for (PhysicsObject* object : m_objects)
	{
		if (object == &currentObject || !CheckCollisionCallbacks(currentObject, *object))
			continue;
		
		std::optional<glm::vec3> correction;
		
		if (const eg::CollisionMesh* const* mesh = std::get_if<const eg::CollisionMesh*>(&object->shape))
		{
			glm::mat4 meshTransform = glm::translate(glm::mat4_cast(object->rotation), object->position);
			correction = CheckCollisionAABBTriangleMesh(shiftedAABB, currentObject.move, **mesh, meshTransform);
		}
		else if (const eg::AABB* otherAABB = std::get_if<eg::AABB>(&object->shape))
		{
			OrientedBox otherOBB;
			otherOBB.center = otherAABB->Center() + object->position;
			otherOBB.radius = otherAABB->Size() / 2.0f;
			otherOBB.rotation = object->rotation;
			correction = CheckCollisionAABBOrientedBox(shiftedAABB, otherOBB, currentObject.move);
		}
		
		if (combiner.Update(correction))
		{
			otherObject = object;
		}
	}
	
	CheckCollisionResult result;
	if (combiner.GetCorrection())
	{
		result.collided = true;
		result.other = otherObject;
		result.correction = *combiner.GetCorrection();
	}
	return result;
}

PhysicsEngine::CheckCollisionResult PhysicsEngine::CheckForCollision(const PhysicsObject& currentObject,
	const eg::CollisionMesh* shape, const glm::vec3& position, const glm::quat& rotation)
{
	return { };
}

PhysicsEngine::CheckCollisionResult PhysicsEngine::CheckForCollision(const PhysicsObject& currentObject,
	const glm::vec3& position, const glm::quat& rotation)
{
	return std::visit([&] (const auto& shape)
	{
		return CheckForCollision(currentObject, shape, position, rotation);
	}, currentObject.shape);
}
