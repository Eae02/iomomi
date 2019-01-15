#include "RenderSettings.hpp"

RenderSettings::RenderSettings()
	: m_buffer(eg::BufferUsage::UniformBuffer | eg::BufferUsage::CopyDst, BUFFER_SIZE, nullptr)
{
	
}

void RenderSettings::UpdateBuffer()
{
	eg::BufferRef uploadBuffer = eg::GetTemporaryUploadBuffer(BUFFER_SIZE);
	char* uploadMem = reinterpret_cast<char*>(uploadBuffer.Map(0, BUFFER_SIZE));
	reinterpret_cast<glm::mat4*>(uploadMem)[0] = viewProjection;
	reinterpret_cast<glm::mat4*>(uploadMem)[1] = invViewProjection;
	*reinterpret_cast<glm::vec3*>(uploadMem + 128) = cameraPosition;
	uploadBuffer.Unmap(0, BUFFER_SIZE);
	
	eg::DC.CopyBuffer(uploadBuffer, m_buffer, 0, 0, BUFFER_SIZE);
}
