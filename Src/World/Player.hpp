#pragma once

#include <sstream>

#include "Dir.hpp"
#include "Entities/EntInteractable.hpp"
#include "World.hpp"

class Player
{
public:
	Player();

	void Update(World& world, PhysicsEngine& physicsEngine, float dt, bool underwater);

	void GetViewMatrix(glm::mat4& matrixOut, glm::mat4& inverseMatrixOut) const;

	void GetDebugText(std::ostringstream& stream);

	const glm::vec3& EyePosition() const { return m_eyePosition; }

	const glm::quat& Rotation() const { return m_rotation; }

	const glm::vec3& Velocity() const { return m_physicsObject.velocity; }

	glm::vec3 Forward() const;

	Dir CurrentDown() const { return m_down; }

	eg::AABB GetAABB() const;

	void FlipDown();

	glm::vec3 FeetPosition() const;

	bool OnGround() const { return m_onGround; }

	bool Underwater() const { return m_wasUnderwater; }

	bool CanPickUpCube() const;

	void Reset();

	glm::vec3 Position() const { return m_physicsObject.position; }
	void SetPosition(const glm::vec3& position) { m_physicsObject.position = position; }

	bool IsCarryingAndTouchingGravityCorner() const { return m_isCarryingAndTouchingGravityCorner; }

	static constexpr float HEIGHT = 1.9f;
	static constexpr float WIDTH = 0.8f;
	static constexpr float EYE_HEIGHT = HEIGHT * 0.8f;

	float m_rotationYaw = 0;
	float m_rotationPitch = 0;

	bool m_isCarrying = false;

	std::optional<InteractControlHint> interactControlHint;

private:
	Dir m_down = Dir::NegY;

	float m_eyeOffsetFade = 1;
	float m_onGroundLinger = 0;
	float m_planeMovementDisabledTimer = 0;
	float m_viewBobbingIntensity = 1;
	float m_viewBobbingTime = 0;
	bool m_onGround = false;
	bool m_wasOnGround = false;
	bool m_wasUnderwater = false;
	bool m_wasClimbingLadder = false;
	float m_leftWaterTime = 0;

	bool m_isCarryingAndTouchingGravityCorner = false;

	float m_stepSoundRemDistance = 0;
	int m_nextStepSoundRightIndex = -1;

	enum class TransitionMode
	{
		None,
		Corner,
		Fall
	};

	TransitionMode m_gravityTransitionMode = TransitionMode::None;
	float m_transitionTime;
	glm::vec3 m_oldPosition;
	glm::vec3 m_oldEyePosition;
	glm::vec3 m_newPosition;
	glm::quat m_oldRotation;
	glm::quat m_newRotation;

	glm::vec3 m_eyePosition;
	glm::quat m_rotation;

	std::array<std::tuple<glm::vec3, glm::quat, Dir>, 20> m_onGroundRingBuffer;
	float m_onGroundPushDelay = 0;
	uint32_t m_onGroundRingBufferSize = 0;
	uint32_t m_onGroundRingBufferFront = 0;

	glm::vec3 m_radius;

	PhysicsObject m_physicsObject;
};
