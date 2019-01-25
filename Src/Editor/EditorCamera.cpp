#include "EditorCamera.hpp"

EditorCamera::EditorCamera()
{
	Reset();
}

void EditorCamera::Reset()
{
	m_yaw = eg::PI / 4;
	m_pitch = -eg::PI / 6;
	m_targetDistance = m_distance = 5;
	m_focus = glm::vec3(1.5f);
	UpdateRotationMatrix();
}

void EditorCamera::Update(float dt)
{
	if (eg::IsButtonDown(eg::Button::MouseMiddle))
	{
		eg::SetRelativeMouseMode(true);
		if (eg::InputState::Current().IsShiftDown())
		{
			const float OFFSET_SENSITIVITY = 0.001f;
			
			glm::vec3 offset(m_rotationMatrix * glm::vec4(-eg::CursorDeltaX(), eg::CursorDeltaY(), 0, 1));
			m_focus += offset * OFFSET_SENSITIVITY * m_distance;
		}
		else
		{
			const float MOUSE_SENSITIVITY = -0.005f;
			const double pitch1 = (m_pitch + eg::HALF_PI) / eg::TWO_PI;
			const float yawSign = (pitch1 - std::floor(pitch1)) > 0.5f ? -1 : 1;
			
			m_yaw += eg::CursorDeltaX() * MOUSE_SENSITIVITY * yawSign;
			m_pitch += eg::CursorDeltaY() * MOUSE_SENSITIVITY;
			UpdateRotationMatrix();
		}
	}
	else
	{
		eg::SetRelativeMouseMode(false);
	}
	
	m_targetDistance += (eg::InputState::Previous().scrollY - eg::InputState::Current().scrollY) * m_targetDistance * 0.1f;
	m_targetDistance = std::max(m_targetDistance, 1.0f);
	m_distance += std::min(dt * 10, 1.0f) * (m_targetDistance - m_distance);
}

void EditorCamera::GetViewMatrix(glm::mat4& matrixOut, glm::mat4& inverseMatrixOut) const
{
	matrixOut = glm::translate(glm::mat4(1), glm::vec3(0, 0, -m_distance)) * glm::transpose(m_rotationMatrix) * glm::translate(glm::mat4(1), -m_focus);
	inverseMatrixOut = glm::translate(glm::mat4(1), m_focus) * m_rotationMatrix * glm::translate(glm::mat4(1), glm::vec3(0, 0, m_distance));
}

void EditorCamera::UpdateRotationMatrix()
{
	m_rotationMatrix = glm::rotate(glm::mat4(1), m_yaw, glm::vec3(0, 1, 0)) *
		glm::rotate(glm::mat4(1), m_pitch, glm::vec3(1, 0, 0));
}
