#pragma once

#include <SDL2/SDL.h>

enum class MouseButtons
{
	Left = SDL_BUTTON_LEFT,
	Right = SDL_BUTTON_RIGHT,
	Middle = SDL_BUTTON_MIDDLE,
	X1 = SDL_BUTTON_X1,
	X2 = SDL_BUTTON_X2
};

class InputState
{
public:
	InputState();
	
	bool IsMouseButtonDown(MouseButtons button) const
	{
		return m_currentFrame.m_mouseButtonsDown[static_cast<int>(button)];
	}
	
	bool WasMouseButtonDown(MouseButtons button) const
	{
		return m_prevFrame.m_mouseButtonsDown[static_cast<int>(button)];
	}
	
	bool IsMouseButtonClicked(MouseButtons button) const
	{
		return IsMouseButtonDown(button) && !WasMouseButtonDown(button);
	}
	
	bool IsKeyDown(SDL_Scancode key) const
	{
		return m_currentFrame.m_keysDown[key];
	}
	
	bool WasKeyDown(SDL_Scancode key) const
	{
		return m_prevFrame.m_keysDown[key];
	}
	
	const glm::vec2& CursorPos() const
	{
		return m_currentFrame.m_cursorPos;
	}
	
	const glm::vec2& PrevCursorPos() const
	{
		return m_prevFrame.m_cursorPos;
	}
	
	glm::vec2 CursorDelta() const
	{
		return m_cursorDelta;
	}
	
	int MouseWheelDelta() const
	{
		return m_wheelDelta;
	}
	
	void NewFrame();
	
	void MouseWheelEvent(const SDL_MouseWheelEvent& event);
	
	void MouseMotionEvent(const SDL_MouseMotionEvent& event);
	
	void MouseButtonEvent(MouseButtons button, bool newState);
	
	void KeyEvent(SDL_Scancode key, bool newState);
	
protected:
	inline void SetCursorPos(const glm::vec2& cursorPos)
	{
		m_currentFrame.m_cursorPos = m_prevFrame.m_cursorPos = cursorPos;
	}
	
	struct FrameState
	{
		glm::vec2 m_cursorPos;
		std::array<bool, 3> m_mouseButtonsDown;
		std::array<bool, SDL_NUM_SCANCODES> m_keysDown;
	};
	
	FrameState m_currentFrame;
	FrameState m_prevFrame;
	
	int m_wheelDelta = 0;
	glm::vec2 m_cursorDelta;
};
