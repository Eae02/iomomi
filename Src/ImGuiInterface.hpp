#pragma once

#include <imgui.h>

#ifndef IOMOMI_NO_EDITOR

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

inline ImTextureID MakeImTextureID(eg::TextureViewHandle viewHandle)
{
	static_assert(sizeof(ImTextureID) == sizeof(eg::TextureViewHandle));
	return reinterpret_cast<ImTextureID>(viewHandle);
}

void ImPushDisabled(bool disabled);
void ImPopDisabled(bool disabled);

#else

inline ImTextureID MakeImTextureID(eg::TextureRef texture, int layer = -1) { return { }; }
inline void ImPushDisabled(bool disabled) { }
inline void ImPopDisabled(bool disabled) { }

#endif
