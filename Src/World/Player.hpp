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
	
private:
	static constexpr float HEIGHT = 1.65f;
	static constexpr float WIDTH = 0.8f;
	static constexpr float EYE_HEIGHT = HEIGHT * 0.75f;
	
	bool m_onGround = false;
	
	Dir m_down = Dir::NegY;
	const GravityCorner* m_currentCorner = nullptr;
	
	float m_transitionTime;
	glm::vec3 m_oldEyePosition;
	glm::vec3 m_newPosition;
	glm::quat m_oldRotation;
	glm::quat m_newRotation;
	
	glm::vec3 m_position;
	glm::vec3 m_eyePosition;
	glm::vec3 m_velocity;
	glm::quat m_rotation;
	
	float m_rotationYaw = 0;
	float m_rotationPitch = 0;
};
