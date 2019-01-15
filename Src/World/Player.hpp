#pragma once

#include <EGame/EG.hpp>

#include "World.hpp"

class Player
{
public:
	Player();
	
	void Update(World& world, float dt);
	
	void GetViewMatrix(glm::mat4& matrixOut, glm::mat4& inverseMatrixOut) const;
	
private:
	static constexpr float HEIGHT = 0.9f;
	static constexpr float WIDTH = 0.3f;
	static constexpr float EYE_HEIGHT = HEIGHT * 0.75f;
	
	bool m_onGround = false;
	
	glm::vec3 m_position;
	glm::vec3 m_velocity;
	
	glm::mat3 m_rotationMatrix;
	
	float m_rotationYaw = 0;
	float m_rotationPitch = 0;
};
