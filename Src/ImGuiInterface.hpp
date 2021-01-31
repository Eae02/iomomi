#pragma once

#include <imgui.h>
#include "Graphics/GraphicsCommon.hpp"

class ImGuiInterface
{
public:
	ImGuiInterface();
	
	void NewFrame();
	
	void EndFrame();
	
private:
	std::chrono::high_resolution_clock::time_point m_lastFrameBegin;
	
	eg::EventListener<eg::ButtonEvent> m_buttonEventListener;
	
	std::string m_iniFileName;
	
	eg::Texture m_fontTexture;
	
	eg::Pipeline m_pipeline;
	
	ResizableBuffer m_vertexBuffer;
	ResizableBuffer m_indexBuffer;
};

ImTextureID MakeImTextureID(eg::TextureRef texture, int layer = -1);

void ImPushDisabled(bool disabled);
void ImPopDisabled(bool disabled);
