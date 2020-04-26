#pragma once

#include <any>
#include <variant>

using CollisionShape = std::variant<eg::AABB, const eg::CollisionMesh*>;

class PhysicsObject
{
public:
	friend class PhysicsEngine;
	
	glm::vec3 position;
	glm::quat rotation;
	glm::vec3 gravity;
	glm::vec3 force;
	glm::vec3 velocity;
	glm::vec3 move;
	
	glm::vec3 pushForce;
	glm::vec3 displayPosition;
	glm::vec3 previousPosition;
	std::vector<class PhysicsObject*> childObjects;
	
	float mass = 10;
	bool canBePushed = true;
	bool canCarry = false;
	CollisionShape shape;
	std::variant<std::monostate, struct Player*, struct Ent*> owner;
	bool(*shouldCollide)(const PhysicsObject& self, const PhysicsObject& other) = nullptr;
	
private:
	int collisionDepth = 0;
	bool hasCopiedParentMove = false;
};

class PhysicsEngine
{
public:
	
	void Simulate(float dt);
	
	void RegisterObject(PhysicsObject* object);
	
private:
	void CopyParentMove(PhysicsObject& object);
	
	void ApplyMovement(PhysicsObject& object);
	
	struct CheckCollisionResult
	{
		bool collided = false;
		glm::vec3 correction;
		PhysicsObject* other = nullptr;
	};
	
	static bool CheckCollisionCallbacks(const PhysicsObject& a, const PhysicsObject& b);
	
	CheckCollisionResult CheckForCollision(const PhysicsObject& currentObject,
		const glm::vec3& position, const glm::quat& rotation);
	CheckCollisionResult CheckForCollision(const PhysicsObject& currentObject,
		const eg::AABB& shape, const glm::vec3& position, const glm::quat& rotation);
	CheckCollisionResult CheckForCollision(const PhysicsObject& currentObject,
		const eg::CollisionMesh* shape, const glm::vec3& position, const glm::quat& rotation);
	
	std::vector<PhysicsObject*> m_objects;
};
