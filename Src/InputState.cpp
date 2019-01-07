#include "InputState.hpp"
#include "Utils.hpp"

InputState::InputState()
{
	std::fill(std::begin(m_currentFrame.m_keysDown), std::end(m_currentFrame.m_keysDown), false);
	std::fill(std::begin(m_currentFrame.m_mouseButtonsDown), std::end(m_currentFrame.m_mouseButtonsDown), false);
	std::fill(std::begin(m_prevFrame.m_keysDown), std::end(m_prevFrame.m_keysDown), false);
	std::fill(std::begin(m_prevFrame.m_mouseButtonsDown), std::end(m_prevFrame.m_mouseButtonsDown), false);
}

void InputState::NewFrame()
{
	m_prevFrame = m_currentFrame;
	m_cursorDelta = glm::vec2();
	m_wheelDelta = 0;
}

void InputState::MouseWheelEvent(const SDL_MouseWheelEvent& event)
{
	m_wheelDelta += event.y;
}

void InputState::MouseMotionEvent(const SDL_MouseMotionEvent& event)
{
	m_currentFrame.m_cursorPos.x = event.x;
	m_currentFrame.m_cursorPos.y = event.y;
	
	m_cursorDelta.x += event.xrel;
	m_cursorDelta.y += event.yrel;
}

void InputState::MouseButtonEvent(MouseButtons button, bool newState)
{
	m_currentFrame.m_mouseButtonsDown[(int)button - SDL_BUTTON_LEFT] = newState;
}

void InputState::KeyEvent(SDL_Scancode key, bool newState)
{
	m_currentFrame.m_keysDown[key] = newState;
}
