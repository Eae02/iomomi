#pragma once

#include "World.hpp"
#include "Dir.hpp"

class Player
{
public:
	Player();
	
	void Update(World& world, float dt);
	
	void GetViewMatrix(glm::mat4& matrixOut, glm::mat4& inverseMatrixOut) const;
	
	void DebugDraw();
	
	const glm::vec3& EyePosition() const
	{
		return m_eyePosition;
	}
	
	const glm::vec3& Position() const
	{
		return m_position;
	}
	
	glm::vec3 Forward() const;
	
	void SetPosition(const glm::vec3& position)
	{
		m_position = position;
	}
	
	void SetRotation(float yaw, float pitch)
	{
		m_rotationYaw = yaw;
		m_rotationPitch = pitch;
	}
	
	Dir CurrentDown() const
	{
		return m_down;
	}
	
	eg::AABB GetAABB() const
	{
		return eg::AABB(m_position - m_radius, m_position + m_radius);
	}
	
	void FlipDown();
	
	bool OnGround() const
	{
		return m_onGround;
	}
	
	void SetIsCarrying(bool isCarrying)
	{
		m_isCarrying = isCarrying;
	}
	
	bool IsCarrying() const
	{
		return m_isCarrying;
	}
	
	static constexpr float HEIGHT = 1.65f;
	static constexpr float WIDTH = 0.8f;
	static constexpr float EYE_HEIGHT = HEIGHT * 0.75f;
	
private:
	Dir m_down = Dir::NegY;
	
	bool m_onGround = false;
	bool m_isCarrying = false;
	
	enum class TransitionMode
	{
		None,
		Corner,
		Fall
	};
	
	TransitionMode m_gravityTransitionMode = TransitionMode::None;
	float m_transitionTime;
	glm::vec3 m_oldEyePosition;
	glm::vec3 m_newPosition;
	glm::quat m_oldRotation;
	glm::quat m_newRotation;
	
	glm::vec3 m_position;
	glm::vec3 m_eyePosition;
	glm::vec3 m_velocity;
	glm::quat m_rotation;
	
	glm::vec3 m_radius;
	
	float m_rotationYaw = 0;
	float m_rotationPitch = 0;
};
