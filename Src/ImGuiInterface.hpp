#pragma once

#include <imgui.h>

class ImGuiInterface
{
public:
	ImGuiInterface();
	
	void NewFrame();
	
	void EndFrame();
	
	bool IsMouseCaptured();
	bool IsKeyboardCaptured();
	
	void OnTextInput(const char* text);
	
private:
	std::chrono::high_resolution_clock::time_point m_lastFrameBegin;
	
	eg::EventListener<eg::ButtonEvent> m_buttonEventListener;
	
	std::string m_iniFileName;
	
	eg::Texture m_fontTexture;
	
	eg::Pipeline m_pipeline;
	
	int m_vertexCapacity = 0;
	int m_indexCapacity = 0;
	
	eg::Buffer m_vertexBuffer;
	eg::Buffer m_indexBuffer;
	
	bool m_isMouseCaptured = false;
	bool m_isKeyboardCaptured = false;
};
