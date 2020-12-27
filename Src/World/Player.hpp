#pragma once

#include "World.hpp"
#include "Dir.hpp"
#include "Entities/EntInteractable.hpp"

class Player
{
public:
	Player();
	
	void Update(World& world, PhysicsEngine& physicsEngine, float dt, bool underwater);
	
	void GetViewMatrix(glm::mat4& matrixOut, glm::mat4& inverseMatrixOut) const;
	
	void DrawDebugOverlay();
	
	const glm::vec3& EyePosition() const
	{
		return m_eyePosition;
	}
	
	const glm::quat& Rotation() const
	{
		return m_rotation;
	}
	
	const glm::vec3& Velocity() const
	{
		return m_physicsObject.velocity;
	}
	
	glm::vec3 Forward() const;
	
	Dir CurrentDown() const
	{
		return m_down;
	}
	
	eg::AABB GetAABB() const
	{
		return eg::AABB(m_physicsObject.position - m_radius, m_physicsObject.position + m_radius);
	}
	
	void FlipDown();
	
	glm::vec3 FeetPosition() const;
	
	bool OnGround() const
	{
		return m_onGround;
	}
	
	bool Underwater() const
	{
		return m_wasUnderwater;
	}
	
	void Reset();
	
	glm::vec3 Position() const { return m_physicsObject.position; }
	void SetPosition(const glm::vec3& position)
	{
		m_physicsObject.position = position;
	}
	
	static constexpr float HEIGHT = 1.9f;
	static constexpr float WIDTH = 0.8f;
	static constexpr float EYE_HEIGHT = HEIGHT * 0.8f;
	
	float m_rotationYaw = 0;
	float m_rotationPitch = 0;
	
	bool m_isCarrying = false;
	
	std::optional<InteractControlHint> interactControlHint;
	
private:
	Dir m_down = Dir::NegY;
	
	float m_onGroundLinger = 0;
	float m_planeMovementDisabledTimer = 0;
	bool m_onGround = false;
	bool m_wasOnGround = false;
	bool m_wasUnderwater = false;
	
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
