#include "ImGuiInterface.hpp"
#include "Graphics/GL/GL.hpp"
#include "Graphics/Graphics.hpp"

#include <SDL2/SDL.h>
#include <filesystem>
#include <imgui.h>

static const char* vertexShader = R"(#version 330 core
layout(location=0) in vec2 position_in;
layout(location=1) in vec2 texCoord_in;
layout(location=2) in vec4 color_in;

out vec4 vColor;
out vec2 vTexCoord;

uniform vec2 uScale;

void main()
{
	vColor = vec4(color_in.rgb, color_in.a);
	vTexCoord = vec2(texCoord_in.x, texCoord_in.y);
	
	vec2 scaledPos = position_in * uScale;
	gl_Position = vec4(scaledPos.x - 1.0, 1.0 - scaledPos.y, 0, 1);
}
)";

static const char* fragmentShader = R"(#version 330 core
layout(location=0) out vec4 color_out;

in vec4 vColor;
in vec2 vTexCoord;

uniform sampler2D uTexture;

void main()
{
	color_out = vColor;
	color_out.a *= texture(uTexture, vTexCoord).r;
}
)";

static char clipboardTextMem[256];

ImGuiInterface::ImGuiInterface()
	: m_vertexBuffer(0, gl::BufferUsage::Update, nullptr), m_indexBuffer(0, gl::BufferUsage::Update, nullptr)
{
	ImGui::CreateContext();
	
	// ** Initializes ImGui IO **
	ImGuiIO& io = ImGui::GetIO();
	io.KeyMap[ImGuiKey_Tab]        = (int)SDL_SCANCODE_TAB;
	io.KeyMap[ImGuiKey_LeftArrow]  = (int)SDL_SCANCODE_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = (int)SDL_SCANCODE_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow]    = (int)SDL_SCANCODE_UP;
	io.KeyMap[ImGuiKey_DownArrow]  = (int)SDL_SCANCODE_DOWN;
	io.KeyMap[ImGuiKey_PageUp]     = (int)SDL_SCANCODE_PAGEUP;
	io.KeyMap[ImGuiKey_PageDown]   = (int)SDL_SCANCODE_PAGEDOWN;
	io.KeyMap[ImGuiKey_Home]       = (int)SDL_SCANCODE_HOME;
	io.KeyMap[ImGuiKey_End]        = (int)SDL_SCANCODE_END;
	io.KeyMap[ImGuiKey_Delete]     = (int)SDL_SCANCODE_DELETE;
	io.KeyMap[ImGuiKey_Backspace]  = (int)SDL_SCANCODE_BACKSPACE;
	io.KeyMap[ImGuiKey_Enter]      = (int)SDL_SCANCODE_RETURN;
	io.KeyMap[ImGuiKey_Escape]     = (int)SDL_SCANCODE_ESCAPE;
	io.KeyMap[ImGuiKey_Space]      = (int)SDL_SCANCODE_SPACE;
	io.KeyMap[ImGuiKey_A]          = (int)SDL_SCANCODE_A;
	io.KeyMap[ImGuiKey_C]          = (int)SDL_SCANCODE_C;
	io.KeyMap[ImGuiKey_V]          = (int)SDL_SCANCODE_V;
	io.KeyMap[ImGuiKey_X]          = (int)SDL_SCANCODE_X;
	io.KeyMap[ImGuiKey_Y]          = (int)SDL_SCANCODE_Y;
	io.KeyMap[ImGuiKey_Z]          = (int)SDL_SCANCODE_Z;
	
	//m_iniFileName = fs::ExeDirPath() + "/ImGui.ini";
	//io.IniFilename = m_iniFileName.c_str();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	
	ImGui::StyleColorsDark(&ImGui::GetStyle());
	
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
	
	m_shader.AttachStage(GL_VERTEX_SHADER, vertexShader);
	m_shader.AttachStage(GL_FRAGMENT_SHADER, fragmentShader);
	m_shader.Link();
	m_shader.SetUniform("uTexture", 0);
	
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
	
	glGenTextures(1, m_fontTexture.GenPtr());
	glBindTexture(GL_TEXTURE_2D, *m_fontTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, fontTexWidth, fontTexHeight, 0, GL_RED, GL_UNSIGNED_BYTE, fontTexPixels);
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	
	io.Fonts->TexID = (void*)(uintptr_t)*m_fontTexture;
	
	glGenVertexArrays(1, m_vertexArray.GenPtr());
	glBindVertexArray(*m_vertexArray);
	for (GLuint i = 0; i < 3; i++)
		glEnableVertexAttribArray(i);
}

void ImGuiInterface::NewFrame(const InputState& inputState, int drawableWidth, int drawableHeight)
{
	ImGuiIO& io = ImGui::GetIO();
	
	m_isMouseCaptured = io.WantCaptureMouse;
	m_isKeyboardCaptured = io.WantCaptureKeyboard;
	
	using namespace std::chrono;
	high_resolution_clock::time_point time = high_resolution_clock::now();
	
	io.DisplaySize.x = drawableWidth;
	io.DisplaySize.y = drawableHeight;
	io.DeltaTime = duration_cast<nanoseconds>(time - m_lastFrameBegin).count() * 1E-9f;
	io.MousePos = inputState.CursorPos();
	//io.MouseWheel = inputState.ScrollDelta().y;
	//io.MouseWheelH = inputState.ScrollDelta().x;
	
	m_lastFrameBegin = time;
	
	ImGuiMouseCursor cursor = ImGui::GetMouseCursor();
	if (io.MouseDrawCursor || cursor == ImGuiMouseCursor_None)
	{
		SDL_ShowCursor(SDL_FALSE);
	}
	else
	{
		SDL_SetCursor(m_cursors[m_cursors[cursor] ? cursor : ImGuiMouseCursor_Arrow].get());
		SDL_ShowCursor(SDL_TRUE);
	}
	
	ImGui::NewFrame();
}

void ImGuiInterface::OnMouseButtonStateChanged(MouseButtons button, bool pressed)
{
	ImGuiIO& io = ImGui::GetIO();
	switch (button)
	{
	case MouseButtons::Left:
		io.MouseDown[0] = pressed;
		break;
	case MouseButtons::Right:
		io.MouseDown[1] = pressed;
		break;
	case MouseButtons::Middle:
		io.MouseDown[2] = pressed;
		break;
	default: break;
	}
}

void ImGuiInterface::OnKeyStateChanged(SDL_Scancode key, bool pressed)
{
	ImGuiIO& io = ImGui::GetIO();
	
	io.KeysDown[(int)(key)] = pressed;
	
	switch (key)
	{
	case SDL_SCANCODE_LSHIFT:
	case SDL_SCANCODE_RSHIFT:
		io.KeyShift = pressed;
		break;
	case SDL_SCANCODE_LCTRL:
	case SDL_SCANCODE_RCTRL:
		io.KeyCtrl = pressed;
		break;
	case SDL_SCANCODE_LALT:
	case SDL_SCANCODE_RALT:
		io.KeyAlt = pressed;
		break;
	default:
		break;
	}
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
		m_vertexCapacity = RoundToNextMultiple(drawData->TotalVtxCount, 1024);
		m_vertexBuffer.Realloc(m_vertexCapacity * sizeof(ImDrawVert) * NUM_VIRTUAL_FRAMES, nullptr);
	}
	
	//Create the index buffer if it's too small or hasn't been created yet
	if (m_indexCapacity < drawData->TotalIdxCount)
	{
		m_indexCapacity = RoundToNextMultiple(drawData->TotalIdxCount, 1024);
		m_indexBuffer.Realloc(m_indexCapacity * sizeof(ImDrawIdx) * NUM_VIRTUAL_FRAMES, nullptr);
	}
	
	//Writes vertices
	uint64_t vertexBufferOffset = vFrameIndex * m_vertexCapacity * sizeof(ImDrawVert);
	uint64_t vertexCount = 0;
	for (int n = 0; n < drawData->CmdListsCount; n++)
	{
		const ImDrawList* cmdList = drawData->CmdLists[n];
		m_vertexBuffer.Update(vertexBufferOffset + vertexCount * sizeof(ImDrawVert),
		                      cmdList->VtxBuffer.Size * sizeof(ImDrawVert), cmdList->VtxBuffer.Data);
		vertexCount += cmdList->VtxBuffer.Size;
	}
	
	//Writes indices
	uint64_t indexBufferOffset = vFrameIndex * m_indexCapacity * sizeof(ImDrawIdx);
	uint32_t indexCount = 0;
	for (int n = 0; n < drawData->CmdListsCount; n++)
	{
		const ImDrawList* cmdList = drawData->CmdLists[n];
		m_indexBuffer.Update(indexBufferOffset + indexCount * sizeof(uint16_t),
		                     cmdList->IdxBuffer.Size * sizeof(uint16_t), cmdList->IdxBuffer.Data);
		indexCount += cmdList->IdxBuffer.Size;
	}
	
	m_shader.Bind();
	
	glBindVertexArray(*m_vertexArray);
	
	m_shader.SetUniform("uScale", glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y));
	
	glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
	
	gl::SetEnabled<GL_BLEND>(true);
	gl::SetEnabled<GL_SCISSOR_TEST>(true);
	
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glActiveTexture(GL_TEXTURE0);
	
	glBindVertexArray(*m_vertexArray);
	glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer.Handle());
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer.Handle());
	
	//Renders the command lists
	for (int n = 0; n < drawData->CmdListsCount; n++)
	{
		const ImDrawList* commandList = drawData->CmdLists[n];
		
		void* posOffsetP = reinterpret_cast<void*>((uintptr_t)(vertexBufferOffset + offsetof(ImDrawVert, pos)));
		void* uvOffsetP = reinterpret_cast<void*>((uintptr_t)(vertexBufferOffset + offsetof(ImDrawVert, uv)));
		void* colOffsetP = reinterpret_cast<void*>((uintptr_t)(vertexBufferOffset + offsetof(ImDrawVert, col)));
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), posOffsetP);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), uvOffsetP);
		glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), colOffsetP);
		
		for (const ImDrawCmd& drawCommand : commandList->CmdBuffer)
		{
			GLuint tex = (GLuint)reinterpret_cast<uintptr_t>(drawCommand.TextureId);
			glBindTexture(GL_TEXTURE_2D, tex);
			
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
				
				glScissor(scissorX, scissorY, scissorW, scissorH);
				
				glDrawElements(GL_TRIANGLES, drawCommand.ElemCount, GL_UNSIGNED_SHORT, (void*)indexBufferOffset);
			}
			indexBufferOffset += drawCommand.ElemCount * sizeof(uint16_t);
		}
		vertexBufferOffset += commandList->VtxBuffer.Size * sizeof(ImDrawVert);
	}
	
	gl::SetEnabled<GL_SCISSOR_TEST>(false);
	gl::SetEnabled<GL_BLEND>(false);
}

void ImGuiInterface::OnTextInput(const char* text)
{
	ImGui::GetIO().AddInputCharactersUTF8(text);
}
