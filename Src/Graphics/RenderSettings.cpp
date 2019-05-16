#include "RenderSettings.hpp"

RenderSettings* RenderSettings::instance;

RenderSettings::RenderSettings()
{
	eg::BufferCreateInfo createInfo;
	createInfo.flags = eg::BufferFlags::UniformBuffer | eg::BufferFlags::CopyDst;
	createInfo.size = BUFFER_SIZE;
	createInfo.label = "RenderSettings";
	m_buffer = eg::Buffer(createInfo);
}

void RenderSettings::UpdateBuffer()
{
	eg::UploadBuffer uploadBuffer = eg::GetTemporaryUploadBuffer(BUFFER_SIZE);
	char* uploadMem = reinterpret_cast<char*>(uploadBuffer.Map());
	reinterpret_cast<glm::mat4*>(uploadMem)[0] = viewProjection;
	reinterpret_cast<glm::mat4*>(uploadMem)[1] = invViewProjection;
	*reinterpret_cast<glm::vec3*>(uploadMem + 128) = cameraPosition;
	*reinterpret_cast<float*>(uploadMem + 140) = gameTime;
	uploadBuffer.Flush();
	
	eg::DC.CopyBuffer(uploadBuffer.buffer, m_buffer, uploadBuffer.offset, 0, BUFFER_SIZE);
	
	m_buffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Vertex | eg::ShaderAccessFlags::Fragment);
}
