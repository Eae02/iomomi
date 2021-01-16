#pragma once

class EditorCamera
{
public:
	EditorCamera();
	
	void Reset(const eg::Sphere& levelSphere);
	
	void Update(float dt, bool& canUpdateInput);
	
	void GetViewMatrix(glm::mat4& matrixOut, glm::mat4& inverseMatrixOut) const;
	
private:
	void UpdateRotationMatrix();
	
	float m_yaw;
	float m_pitch;
	
	float m_kbVel[2];
	
	float m_targetDistance;
	float m_distance;
	
	glm::mat4 m_rotationMatrix;
	
	glm::vec3 m_focus;
};
