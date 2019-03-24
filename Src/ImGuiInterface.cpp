#include "ImGuiInterface.hpp"

#include <filesystem>
#include <imgui.h>

static std::string clipboardText;

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
	
	m_iniFileName = eg::ExeRelPath("ImGui.ini");
	io.IniFilename = m_iniFileName.c_str();
	
	ImGui::StyleColorsDark(&ImGui::GetStyle());
	
	io.SetClipboardTextFn = [] (void* data, const char* text) { eg::SetClipboardText(text); };
	io.GetClipboardTextFn = [] (void* data) -> const char* {
		clipboardText = eg::GetClipboardText();
		return clipboardText.c_str();
	};
	
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModule>("Shaders/ImGui.vs.glsl").Handle();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModule>("Shaders/ImGui.fs.glsl").Handle();
	pipelineCI.enableScissorTest = true;
	pipelineCI.blendStates[0] = eg::AlphaBlend;
	pipelineCI.vertexBindings[0] = { sizeof(ImDrawVert), eg::InputRate::Vertex };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 2, (uint32_t)offsetof(ImDrawVert, pos) };
	pipelineCI.vertexAttributes[1] = { 0, eg::DataType::Float32, 2, (uint32_t)offsetof(ImDrawVert, uv) };
	pipelineCI.vertexAttributes[2] = { 0, eg::DataType::UInt8Norm, 4, (uint32_t)offsetof(ImDrawVert, col) };
	
	m_pipeline = eg::Pipeline::Create(pipelineCI);
	m_pipeline.FramebufferFormatHint(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil);
	
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
	
	uint64_t fontTexBytes = fontTexWidth * fontTexHeight;
	eg::Buffer fontUploadBuffer(eg::BufferFlags::CopySrc | eg::BufferFlags::HostAllocate | eg::BufferFlags::MapWrite, fontTexBytes, nullptr);
	void* fontUploadMem = fontUploadBuffer.Map(0, fontTexBytes);
	std::memcpy(fontUploadMem, fontTexPixels, fontTexBytes);
	fontUploadBuffer.Unmap(0, fontTexBytes);
	
	eg::SamplerDescription fontTexSampler;
	fontTexSampler.wrapU = eg::WrapMode::ClampToEdge;
	fontTexSampler.wrapV = eg::WrapMode::ClampToEdge;
	
	eg::Texture2DCreateInfo fontTexCreateInfo;
	fontTexCreateInfo.flags = eg::TextureFlags::CopyDst | eg::TextureFlags::ShaderSample;
	fontTexCreateInfo.width = fontTexWidth;
	fontTexCreateInfo.height = fontTexHeight;
	fontTexCreateInfo.format = eg::Format::R8_UNorm;
	fontTexCreateInfo.defaultSamplerDescription = &fontTexSampler;
	fontTexCreateInfo.mipLevels = 1;
	fontTexCreateInfo.swizzleR = eg::SwizzleMode::One;
	fontTexCreateInfo.swizzleG = eg::SwizzleMode::One;
	fontTexCreateInfo.swizzleB = eg::SwizzleMode::One;
	fontTexCreateInfo.swizzleA = eg::SwizzleMode::R;
	
	m_fontTexture = eg::Texture::Create2D(fontTexCreateInfo);
	eg::DC.SetTextureData(m_fontTexture, { 0, 0, 0, (uint32_t)fontTexWidth, (uint32_t)fontTexHeight, 1, 0 }, fontUploadBuffer, 0);
	io.Fonts->TexID = &m_fontTexture;
	
	m_fontTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
}

void ImGuiInterface::NewFrame()
{
	ImGuiIO& io = ImGui::GetIO();
	
	using namespace std::chrono;
	high_resolution_clock::time_point time = high_resolution_clock::now();
	
	io.DisplaySize.x = eg::CurrentResolutionX();
	io.DisplaySize.y = eg::CurrentResolutionY();
	io.DeltaTime = duration_cast<nanoseconds>(time - m_lastFrameBegin).count() * 1E-9f;
	io.MousePos = ImVec2(eg::CursorPos().x, eg::CursorPos().y);
	io.MouseWheel = eg::InputState::Current().scrollY - eg::InputState::Previous().scrollY;
	io.MouseWheelH = eg::InputState::Current().scrollX - eg::InputState::Previous().scrollX;
	
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
	
	if (!eg::InputtedText().empty())
	{
		ImGui::GetIO().AddInputCharactersUTF8(eg::InputtedText().c_str());
	}
	
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

void ImGuiInterface::EndFrame()
{
	ImGui::Render();
	
	ImDrawData* drawData = ImGui::GetDrawData();
	if (drawData->TotalIdxCount == 0)
		return;
	
	ImGuiIO& io = ImGui::GetIO();
	
	static_assert(sizeof(ImDrawIdx) == sizeof(uint16_t));
	uint64_t verticesBytes = drawData->TotalVtxCount * sizeof(ImDrawVert);
	uint64_t indicesBytes = drawData->TotalIdxCount * sizeof(ImDrawIdx);
	
	//Create the vertex buffer if it's too small or hasn't been created yet
	if (m_vertexCapacity < drawData->TotalVtxCount)
	{
		m_vertexCapacity = eg::RoundToNextMultiple(drawData->TotalVtxCount, 1024);
		m_vertexBuffer = eg::Buffer(eg::BufferFlags::VertexBuffer | eg::BufferFlags::CopyDst,
			m_vertexCapacity * sizeof(ImDrawVert), nullptr);
	}
	
	//Create the index buffer if it's too small or hasn't been created yet
	if (m_indexCapacity < drawData->TotalIdxCount)
	{
		m_indexCapacity = eg::RoundToNextMultiple(drawData->TotalIdxCount, 1024);
		m_indexBuffer = eg::Buffer(eg::BufferFlags::IndexBuffer | eg::BufferFlags::CopyDst,
			m_indexCapacity * sizeof(ImDrawIdx), nullptr);
	}
	
	size_t uploadBufferSize = verticesBytes + indicesBytes;
	eg::UploadBuffer uploadBuffer = eg::GetTemporaryUploadBuffer(uploadBufferSize);
	char* uploadMem = reinterpret_cast<char*>(uploadBuffer.Map());
	
	//Writes vertices
	uint64_t vertexCount = 0;
	ImDrawVert* verticesMem = reinterpret_cast<ImDrawVert*>(uploadMem);
	for (int n = 0; n < drawData->CmdListsCount; n++)
	{
		const ImDrawList* cmdList = drawData->CmdLists[n];
		std::copy_n(cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size, verticesMem + vertexCount);
		vertexCount += cmdList->VtxBuffer.Size;
	}
	
	//Writes indices
	uint32_t indexCount = 0;
	ImDrawIdx* indicesMem = reinterpret_cast<ImDrawIdx*>(uploadMem + verticesBytes);
	for (int n = 0; n < drawData->CmdListsCount; n++)
	{
		const ImDrawList* cmdList = drawData->CmdLists[n];
		std::copy_n(cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size, indicesMem + indexCount);
		indexCount += cmdList->IdxBuffer.Size;
	}
	
	uploadBuffer.Unmap();
	
	eg::DC.CopyBuffer(uploadBuffer.buffer, m_vertexBuffer, uploadBuffer.offset, 0, verticesBytes);
	eg::DC.CopyBuffer(uploadBuffer.buffer, m_indexBuffer, uploadBuffer.offset + verticesBytes, 0, indicesBytes);
	
	m_vertexBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
	m_indexBuffer.UsageHint(eg::BufferUsage::IndexBuffer);
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Load;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Load;
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	eg::DC.BindPipeline(m_pipeline);
	
	float scale[] = { 2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y };
	eg::DC.PushConstants(0, scale);
	
	eg::DC.BindVertexBuffer(0, m_vertexBuffer, 0);
	eg::DC.BindIndexBuffer(eg::IndexType::UInt16, m_indexBuffer, 0);
	
	//Renders the command lists
	uint32_t firstIndex = 0;
	uint32_t firstVertex = 0;
	for (int n = 0; n < drawData->CmdListsCount; n++)
	{
		const ImDrawList* commandList = drawData->CmdLists[n];
		
		for (const ImDrawCmd& drawCommand : commandList->CmdBuffer)
		{
			eg::DC.BindTexture(*reinterpret_cast<eg::Texture*>(drawCommand.TextureId), 0, 0);
			
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
				if (scissorW > 0 && scissorH > 0)
				{
					eg::DC.SetScissor(scissorX, scissorY, scissorW, scissorH);
					eg::DC.DrawIndexed(firstIndex, drawCommand.ElemCount, firstVertex, 0, 1);
				}
			}
			firstIndex += drawCommand.ElemCount;
		}
		firstVertex += commandList->VtxBuffer.Size;
	}
	
	eg::DC.EndRenderPass();
}
