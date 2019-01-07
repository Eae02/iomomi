#pragma once

#include "Graphics/GL/GL.hpp"
#include "Graphics/GL/Shader.hpp"
#include "Graphics/GL/Buffer.hpp"
#include "InputState.hpp"

#include <imgui.h>

class ImGuiInterface
{
public:
	ImGuiInterface();
	
	ImGuiInterface(ImGuiInterface&& other) = delete;
	ImGuiInterface& operator=(ImGuiInterface&& other) = delete;
	ImGuiInterface(const ImGuiInterface& other) = delete;
	ImGuiInterface& operator=(const ImGuiInterface& other) = delete;
	
	void NewFrame(const InputState& inputState, int drawableWidth, int drawableHeight);
	
	void EndFrame(uint32_t vFrameIndex);
	
	bool IsMouseCaptured();
	bool IsKeyboardCaptured();
	
	void OnMouseButtonStateChanged(MouseButtons button, bool pressed);
	void OnKeyStateChanged(SDL_Scancode key, bool pressed);
	void OnTextInput(const char* text);
	
private:
	std::chrono::high_resolution_clock::time_point m_lastFrameBegin;
	
	//std::string m_iniFileName;
	
	gl::Handle<gl::ResType::Texture> m_fontTexture;
	
	gl::Shader m_shader;
	
	gl::Handle<gl::ResType::VertexArray> m_vertexArray;
	
	int m_vertexCapacity = 0;
	int m_indexCapacity = 0;
	
	gl::Buffer m_vertexBuffer;
	gl::Buffer m_indexBuffer;
	
	bool m_isMouseCaptured = false;
	bool m_isKeyboardCaptured = false;
	
	struct CursorFree
	{
		void operator()(SDL_Cursor* c)
		{
			SDL_FreeCursor(c);
		}
	};
	
	std::unique_ptr<SDL_Cursor, CursorFree> m_cursors[ImGuiMouseCursor_COUNT];
};
