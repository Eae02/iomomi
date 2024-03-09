#include "EditorCamera.hpp"
#include "../Settings.hpp"

EditorCamera::EditorCamera()
{
	Reset(eg::Sphere(glm::vec3(0.0f), 5.0f));
}

void EditorCamera::Reset(const eg::Sphere& levelSphere)
{
	m_yaw = eg::PI / 4;
	m_pitch = -eg::PI / 6;
	m_kbVel[0] = 0;
	m_kbVel[1] = 0;
	m_targetDistance = m_distance = levelSphere.radius * 1.1f;
	m_focus = levelSphere.position;
	UpdateRotationMatrix();
}

void EditorCamera::Update(float dt, bool& canUpdateInput)
{
	auto YawSign = [&]
	{
		const double pitch1 = (m_pitch + eg::HALF_PI) / eg::TWO_PI;
		return (pitch1 - std::floor(pitch1)) > 0.5f ? -1 : 1;
	};

	if (eg::IsButtonDown(eg::Button::MouseRight))
	{
		m_isRotating = true;

		glm::vec2 cursorDelta = eg::CursorPosDelta() / eg::DisplayScaleFactor();

		canUpdateInput = false;
		eg::SetRelativeMouseMode(true);
		if (eg::InputState::Current().IsShiftDown())
		{
			const float OFFSET_SENSITIVITY = 0.002f;

			glm::vec3 offset(m_rotationMatrix * glm::vec4(-cursorDelta.x, cursorDelta.y, 0, 1));
			m_focus += offset * OFFSET_SENSITIVITY * m_distance * settings.editorMouseSensitivity;
		}
		else
		{
			const float ROTATE_SENSITIVITY = -0.003f;
			m_yaw +=
				static_cast<float>(cursorDelta.x * YawSign()) * ROTATE_SENSITIVITY * settings.editorMouseSensitivity;
			m_pitch += static_cast<float>(cursorDelta.y) * ROTATE_SENSITIVITY * settings.editorMouseSensitivity;
		}
	}
	else
	{
		m_isRotating = false;
		eg::SetRelativeMouseMode(false);
	}

	float kbRotateAccel = dt * 15.0f;

	auto UpdateKBInput = [&](bool pos, bool neg, float& vel)
	{
		const float MAX_KB_VEL = 2.0f;
		if (neg && !pos)
			vel = std::max(vel - kbRotateAccel, -MAX_KB_VEL);
		else if (!neg && pos)
			vel = std::min(vel + kbRotateAccel, MAX_KB_VEL);
		else if (vel < 0)
			vel = std::min(vel + kbRotateAccel, 0.0f);
		else
			vel = std::max(vel - kbRotateAccel, 0.0f);
	};

	UpdateKBInput(eg::IsButtonDown(eg::Button::RightArrow), eg::IsButtonDown(eg::Button::LeftArrow), m_kbVel[0]);
	UpdateKBInput(eg::IsButtonDown(eg::Button::DownArrow), eg::IsButtonDown(eg::Button::UpArrow), m_kbVel[1]);

	if (eg::InputState::Current().IsShiftDown())
	{
		const float OFFSET_SENSITIVITY = 0.7f;
		glm::vec3 offset(m_rotationMatrix * glm::vec4(m_kbVel[0], -m_kbVel[1], 0, 1));
		m_focus += offset * OFFSET_SENSITIVITY * m_distance * dt;
	}
	else
	{
		m_yaw += m_kbVel[0] * dt * static_cast<float>(YawSign());
		m_pitch += m_kbVel[1] * dt;
	}

	UpdateRotationMatrix();

	m_targetDistance *=
		1.0f + static_cast<float>(eg::InputState::Previous().scrollY - eg::InputState::Current().scrollY) * 0.1f;
	m_targetDistance = std::max(m_targetDistance, 1.0f);
	m_distance += std::min(dt * 10, 1.0f) * (m_targetDistance - m_distance);
}

void EditorCamera::GetViewMatrix(glm::mat4& matrixOut, glm::mat4& inverseMatrixOut) const
{
	matrixOut = glm::translate(glm::mat4(1), glm::vec3(0, 0, -m_distance)) * glm::transpose(m_rotationMatrix) *
	            glm::translate(glm::mat4(1), -m_focus);

	inverseMatrixOut = glm::translate(glm::mat4(1), m_focus) * m_rotationMatrix *
	                   glm::translate(glm::mat4(1), glm::vec3(0, 0, m_distance));
}

void EditorCamera::UpdateRotationMatrix()
{
	m_rotationMatrix =
		glm::rotate(glm::mat4(1), m_yaw, glm::vec3(0, 1, 0)) * glm::rotate(glm::mat4(1), m_pitch, glm::vec3(1, 0, 0));
}
