#pragma once

#include <any>
#include <variant>

using CollisionShape = std::variant<eg::AABB, const eg::CollisionMesh*>;

constexpr uint32_t RAY_MASK_BLOCK_PICK_UP = 2;
constexpr uint32_t RAY_MASK_BLOCK_GUN = 1;

constexpr float GRAVITY_MAG = 20;

class PhysicsObject
{
public:
	friend class PhysicsEngine;
	
	//Set by the physics engine and externally
	glm::vec3 position;
	glm::quat rotation;
	glm::vec3 gravity;
	glm::vec3 force; //reset after each frame
	glm::vec3 velocity;
	glm::vec3 pendingVelocity; //reset after each frame
	glm::vec3 move; //reset after each frame
	glm::quat angularForce; //reset after each frame
	glm::quat angularVelocity;
	glm::quat angularMove; //reset after each frame
	
	//Set only by the physics engine
	glm::vec3 pushForce;
	glm::vec3 actualMove;
	glm::vec3 displayPosition;
	std::vector<class PhysicsObject*> childObjects;
	bool didMove = false;
	
	//Object properties
	bool canBePushed = true;
	bool canCarry = false;
	glm::vec3 slideDim { 1, 1, 1 };
	float mass = 10;
	float friction = 0.5f;
	uint32_t debugColor = 0xde921f;
	CollisionShape shape;
	std::variant<std::monostate, struct World*, struct Player*, struct Ent*> owner;
	uint32_t rayIntersectMask = 0xFF;
	bool(*shouldCollide)(const PhysicsObject& self, const PhysicsObject& other) = nullptr;
	glm::vec3(*constrainMove)(const PhysicsObject& self, const glm::vec3& move) = nullptr;
	
private:
	int collisionDepth = 0;
	bool hasCopiedParentMove = false;
	bool needsFlippedWinding = false;
	bool firstFrame = true;
	
	PhysicsObject* floor = nullptr;
	
	glm::vec3 lockedDisplayPosition;
	float timeUntilLockDisplayPosition = 0;
};

class PhysicsEngine
{
public:
	void BeginCollect();
	
	void EndCollect();
	
	void Simulate(float dt);
	
	void RegisterObject(PhysicsObject* object);
	
	using CollisionCallback = std::function<void(PhysicsObject& other, const glm::vec3& correction)>;
	
	void ApplyMovement(PhysicsObject& object, float dt, const CollisionCallback& callback = nullptr);
	
	PhysicsObject* FindFloorObject(PhysicsObject& object, const glm::vec3& down) const;
	
	//Finds the closest object intersecting the ray. If no intersection, returns (nullptr, infinity)
	std::pair<PhysicsObject*, float> RayIntersect(const eg::Ray& ray, uint32_t mask = 0xFF,
		const PhysicsObject* ignoreObject = nullptr) const;
	
	void GetDebugRenderData(struct PhysicsDebugRenderData& dataOut) const;
	
	PhysicsObject* CheckCollision(const eg::AABB& aabb, const PhysicsObject* ignoreObject) const;
	
private:
	void CopyParentMove(PhysicsObject& object, float dt);
	
	struct CheckCollisionResult
	{
		bool collided = false;
		glm::vec3 correction;
		PhysicsObject* other = nullptr;
	};
	
	static bool CheckCollisionCallbacks(const PhysicsObject& a, const PhysicsObject& b);
	
	CheckCollisionResult CheckForCollision(const PhysicsObject& currentObject,
		const glm::vec3& position, const glm::quat& rotation) const;
	PhysicsObject* CheckForCollision(struct CollisionResponseCombiner& combiner, const PhysicsObject& currentObject,
		const eg::AABB& shape, const glm::vec3& position, const glm::quat& rotation) const;
	PhysicsObject* CheckForCollision(struct CollisionResponseCombiner& combiner, const PhysicsObject& currentObject,
		const eg::CollisionMesh* shape, const glm::vec3& position, const glm::quat& rotation) const;
	
	std::vector<PhysicsObject*> m_objects;
};
