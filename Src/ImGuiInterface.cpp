#include "ImGuiInterface.hpp"

//#include <SDL2/SDL.h>
#include <filesystem>
#include <imgui.h>

//static char clipboardTextMem[256];

ImGuiInterface::ImGuiInterface()
{
	ImGui::CreateContext();
	
	// ** Initializes ImGui IO **
	ImGuiIO& io = ImGui::GetIO();
	io.KeyMap[ImGuiKey_Tab]        = (int)eg::Button::Tab;
	io.KeyMap[ImGuiKey_LeftArrow]  = (int)eg::Button::LeftArrow;
	io.KeyMap[ImGuiKey_RightArrow] = (int)eg::Button::RightArrow;
	io.KeyMap[ImGuiKey_UpArrow]    = (int)eg::Button::UpArrow;
	io.KeyMap[ImGuiKey_DownArrow]  = (int)eg::Button::DownArrow;
	io.KeyMap[ImGuiKey_PageUp]     = (int)eg::Button::PageUp;
	io.KeyMap[ImGuiKey_PageDown]   = (int)eg::Button::PageDown;
	io.KeyMap[ImGuiKey_Home]       = (int)eg::Button::Home;
	io.KeyMap[ImGuiKey_End]        = (int)eg::Button::End;
	io.KeyMap[ImGuiKey_Delete]     = (int)eg::Button::Delete;
	io.KeyMap[ImGuiKey_Backspace]  = (int)eg::Button::Backspace;
	io.KeyMap[ImGuiKey_Enter]      = (int)eg::Button::Enter;
	io.KeyMap[ImGuiKey_Escape]     = (int)eg::Button::Escape;
	io.KeyMap[ImGuiKey_Space]      = (int)eg::Button::Space;
	io.KeyMap[ImGuiKey_A]          = (int)eg::Button::A;
	io.KeyMap[ImGuiKey_C]          = (int)eg::Button::C;
	io.KeyMap[ImGuiKey_V]          = (int)eg::Button::V;
	io.KeyMap[ImGuiKey_X]          = (int)eg::Button::X;
	io.KeyMap[ImGuiKey_Y]          = (int)eg::Button::Y;
	io.KeyMap[ImGuiKey_Z]          = (int)eg::Button::Z;
	
	//m_iniFileName = fs::ExeDirPath() + "/ImGui.ini";
	//io.IniFilename = m_iniFileName.c_str();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	
	ImGui::StyleColorsDark(&ImGui::GetStyle());
	/*
	io.SetClipboardTextFn = [] (void* data, const char* text) { SDL_SetClipboardText(text); };
	io.GetClipboardTextFn = [] (void* data) -> const char* {
		char* sdlClipboardText = SDL_GetClipboardText();
		size_t len = std::min(std::strlen(sdlClipboardText), sizeof(clipboardTextMem) - 1);
		std::memcpy(clipboardTextMem, sdlClipboardText, len + 1);
		SDL_free(sdlClipboardText);
		return clipboardTextMem;
	};
	
	m_cursors[ImGuiMouseCursor_Arrow].reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW));
	m_cursors[ImGuiMouseCursor_TextInput].reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM));
	m_cursors[ImGuiMouseCursor_ResizeAll].reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL));
	m_cursors[ImGuiMouseCursor_ResizeNS].reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS));
	m_cursors[ImGuiMouseCursor_ResizeEW].reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE));
	m_cursors[ImGuiMouseCursor_ResizeNESW].reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW));
	m_cursors[ImGuiMouseCursor_ResizeNWSE].reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE));
	m_cursors[ImGuiMouseCursor_Hand].reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND));
	*/
	
	eg::ShaderProgram shaderProgram;
	shaderProgram.AddStageFromFile("./Data/Shaders/ImGui.fs.spv");
	shaderProgram.AddStageFromFile("./Data/Shaders/ImGui.vs.spv");
	
	eg::FixedFuncState fixedFuncState;
	fixedFuncState.enableStencilTest = true;
	fixedFuncState.attachments[0].blend = eg::AlphaBlend;
	fixedFuncState.vertexBindings[0] = { sizeof(ImDrawVert), eg::InputRate::Vertex };
	fixedFuncState.vertexAttributes[0] = { 0, eg::DataType::Float32, 2, (uint32_t)offsetof(ImDrawVert, pos) };
	fixedFuncState.vertexAttributes[1] = { 0, eg::DataType::Float32, 2, (uint32_t)offsetof(ImDrawVert, uv) };
	fixedFuncState.vertexAttributes[2] = { 0, eg::DataType::UInt8Norm, 4, (uint32_t)offsetof(ImDrawVert, col) };
	
	m_pipeline = shaderProgram.CreatePipeline(fixedFuncState);
	
	// ** Creates the font texture **
	const char* fontPaths[] = 
	{
#if defined(__linux__)
		"/usr/share/fonts/TTF/DroidSans.ttf",
		"/usr/share/fonts/TTF/DejaVuSans.ttf",
		"/usr/share/fonts/TTF/arial.ttf"
#elif defined(_WIN32)
		"C:\\Windows\\Fonts\\arial.ttf"
#endif
	};
	
	auto fontIt = std::find_if(std::begin(fontPaths), std::end(fontPaths), [] (const char* path)
	{
		return std::filesystem::exists(std::filesystem::u8path(path));
	});
	
	if (fontIt != std::end(fontPaths))
		io.Fonts->AddFontFromFileTTF(*fontIt, 14);
	else
		io.Fonts->AddFontDefault();
	
	unsigned char* fontTexPixels;
	int fontTexWidth, fontTexHeight;
	io.Fonts->GetTexDataAsAlpha8(&fontTexPixels, &fontTexWidth, &fontTexHeight);
	
	eg::Texture2DCreateInfo fontTexCreateInfo;
	fontTexCreateInfo.width = fontTexWidth;
	fontTexCreateInfo.height = fontTexHeight;
	fontTexCreateInfo.format = eg::Format::R8_UNorm;
	fontTexCreateInfo.mipLevels = 1;
	
	m_fontTexture = eg::Texture::Create2D(fontTexCreateInfo, fontTexPixels);
	io.Fonts->TexID = &m_fontTexture;
	/*
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	*/
}

void ImGuiInterface::NewFrame()
{
	ImGuiIO& io = ImGui::GetIO();
	
	m_isMouseCaptured = io.WantCaptureMouse;
	m_isKeyboardCaptured = io.WantCaptureKeyboard;
	
	using namespace std::chrono;
	high_resolution_clock::time_point time = high_resolution_clock::now();
	
	io.DisplaySize.x = eg::CurrentResolutionX();
	io.DisplaySize.y = eg::CurrentResolutionY();
	io.DeltaTime = duration_cast<nanoseconds>(time - m_lastFrameBegin).count() * 1E-9f;
	io.MousePos = ImVec2(eg::CursorPos().x, eg::CursorPos().y);
	io.MouseWheel = eg::InputState::Current().scrollY;
	io.MouseWheelH = eg::InputState::Current().scrollX;
	
	m_buttonEventListener.ProcessAll([&] (const eg::ButtonEvent& event)
	{
		io.KeysDown[(int)(event.button)] = event.newState;
		
		switch (event.button)
		{
		case eg::Button::MouseLeft:
			io.MouseDown[0] = event.newState;
			break;
		case eg::Button::MouseRight:
			io.MouseDown[1] = event.newState;
			break;
		case eg::Button::MouseMiddle:
			io.MouseDown[2] = event.newState;
			break;
		case eg::Button::LeftShift:
		case eg::Button::RightShift:
			io.KeyShift = event.newState;
			break;
		case eg::Button::LeftControl:
		case eg::Button::RightControl:
			io.KeyCtrl = event.newState;
			break;
		case eg::Button::LeftAlt:
		case eg::Button::RightAlt:
			io.KeyAlt = event.newState;
			break;
		default:
			break;
		}
	});
	
	m_lastFrameBegin = time;
	/*
	ImGuiMouseCursor cursor = ImGui::GetMouseCursor();
	if (io.MouseDrawCursor || cursor == ImGuiMouseCursor_None)
	{
		SDL_ShowCursor(SDL_FALSE);
	}
	else
	{
		SDL_SetCursor(m_cursors[m_cursors[cursor] ? cursor : ImGuiMouseCursor_Arrow].get());
		SDL_ShowCursor(SDL_TRUE);
	}*/
	
	ImGui::NewFrame();
}

bool ImGuiInterface::IsMouseCaptured()
{
	return m_isMouseCaptured;
}

bool ImGuiInterface::IsKeyboardCaptured()
{
	return m_isKeyboardCaptured;
}

void ImGuiInterface::EndFrame(uint32_t vFrameIndex)
{
	ImGui::Render();
	
	ImDrawData* drawData = ImGui::GetDrawData();
	if (drawData->TotalIdxCount == 0)
		return;
	
	ImGuiIO& io = ImGui::GetIO();
	
	//Create the vertex buffer if it's too small or hasn't been created yet
	if (m_vertexCapacity < drawData->TotalVtxCount)
	{
		m_vertexCapacity = eg::RoundToNextMultiple(drawData->TotalVtxCount, 1024);
		m_vertexBuffer = eg::Buffer(eg::BufferUsage::VertexBuffer | eg::BufferUsage::MapWrite,
			m_vertexCapacity * sizeof(ImDrawVert) * eg::MAX_CONCURRENT_FRAMES, nullptr);
	}
	
	//Create the index buffer if it's too small or hasn't been created yet
	if (m_indexCapacity < drawData->TotalIdxCount)
	{
		m_indexCapacity = eg::RoundToNextMultiple(drawData->TotalIdxCount, 1024);
		m_indexBuffer = eg::Buffer(eg::BufferUsage::IndexBuffer | eg::BufferUsage::MapWrite,
			m_indexCapacity * sizeof(ImDrawIdx) * eg::MAX_CONCURRENT_FRAMES, nullptr);
	}
	
	//Writes vertices
	uint32_t vertexBufferOffset = vFrameIndex * m_vertexCapacity * sizeof(ImDrawVert);
	uint64_t vertexCount = 0;
	ImDrawVert* verticesMem = reinterpret_cast<ImDrawVert*>(m_vertexBuffer.Map(vertexBufferOffset, m_vertexCapacity * sizeof(ImDrawVert)));
	for (int n = 0; n < drawData->CmdListsCount; n++)
	{
		const ImDrawList* cmdList = drawData->CmdLists[n];
		std::copy_n(cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size, verticesMem + vertexCount);
		vertexCount += cmdList->VtxBuffer.Size;
	}
	m_vertexBuffer.Unmap(0, vertexCount * sizeof(ImDrawVert));
	
	//Writes indices
	uint32_t indexBufferOffset = vFrameIndex * m_indexCapacity * sizeof(ImDrawIdx);
	uint32_t indexCount = 0;
	ImDrawIdx* indicesMem = reinterpret_cast<ImDrawIdx*>(m_indexBuffer.Map(indexBufferOffset, m_indexCapacity * sizeof(ImDrawIdx)));
	for (int n = 0; n < drawData->CmdListsCount; n++)
	{
		const ImDrawList* cmdList = drawData->CmdLists[n];
		std::copy_n(cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size, indicesMem + indexCount);
		indexCount += cmdList->IdxBuffer.Size;
	}
	m_indexBuffer.Unmap(0, indexCount * sizeof(ImDrawIdx));
	
	eg::DC.BindPipeline(m_pipeline);
	
	float scale[] = { 2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y };
	eg::DC.SetUniform("uScale", eg::UniformType::Vec2, scale);
	
	eg::DC.SetViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
	
	eg::DC.BindVertexBuffer(0, m_vertexBuffer, vertexBufferOffset);
	eg::DC.BindIndexBuffer(eg::IndexType::UInt16, m_indexBuffer, indexBufferOffset);
	
	//Renders the command lists
	uint32_t firstIndex = 0;
	uint32_t firstVertex = 0;
	for (int n = 0; n < drawData->CmdListsCount; n++)
	{
		const ImDrawList* commandList = drawData->CmdLists[n];
		
		for (const ImDrawCmd& drawCommand : commandList->CmdBuffer)
		{
			eg::DC.BindTexture(*reinterpret_cast<eg::Texture*>(drawCommand.TextureId), 0);
			
			if (drawCommand.UserCallback != nullptr)
			{
				drawCommand.UserCallback(commandList, &drawCommand);
			}
			else
			{
				int scissorX = (int)(std::max(drawCommand.ClipRect.x, 0.0f));
				int scissorY = (int)(std::max(io.DisplaySize.y - drawCommand.ClipRect.w, 0.0f));
				int scissorW = (int)(std::min(drawCommand.ClipRect.z, io.DisplaySize.x) - scissorX);
				int scissorH = (int)(std::min(drawCommand.ClipRect.w, io.DisplaySize.y) - drawCommand.ClipRect.y + 1);
				if (scissorW <= 0 || scissorH <= 0)
					continue;
				
				eg::DC.SetScissor(scissorX, scissorY, scissorW, scissorH);
				
				eg::DC.DrawIndexed(firstIndex, drawCommand.ElemCount, firstVertex, 1);
			}
			firstIndex += drawCommand.ElemCount;
		}
		firstVertex += commandList->VtxBuffer.Size;
	}
}

void ImGuiInterface::OnTextInput(const char* text)
{
	ImGui::GetIO().AddInputCharactersUTF8(text);
}
