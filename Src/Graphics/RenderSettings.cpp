#include "RenderSettings.hpp"

RenderSettings* RenderSettings::instance;

static const eg::DescriptorSetBinding dsBinding = eg::DescriptorSetBinding{
	.binding = 0,
	.type = eg::BindingType::UniformBuffer,
	.shaderAccess = eg::ShaderAccessFlags::Vertex,
};

RenderSettings::RenderSettings() : m_vertexShaderDescriptorSet({ &dsBinding, 1 })
{
	eg::BufferCreateInfo createInfo;
	createInfo.flags = eg::BufferFlags::UniformBuffer | eg::BufferFlags::CopyDst;
	createInfo.size = BUFFER_SIZE;
	createInfo.label = "RenderSettings";
	m_buffer = eg::Buffer(createInfo);

	m_vertexShaderDescriptorSet.BindUniformBuffer(m_buffer, 0, 0, BUFFER_SIZE);
}

void RenderSettings::UpdateBuffer()
{
	eg::UploadBuffer uploadBuffer = eg::GetTemporaryUploadBuffer(BUFFER_SIZE);
	char* uploadMem = reinterpret_cast<char*>(uploadBuffer.Map());
	reinterpret_cast<glm::mat4*>(uploadMem)[0] = viewProjection;
	reinterpret_cast<glm::mat4*>(uploadMem)[1] = invViewProjection;
	reinterpret_cast<glm::mat4*>(uploadMem)[2] = viewMatrix;
	reinterpret_cast<glm::mat4*>(uploadMem)[3] = projectionMatrix;
	reinterpret_cast<glm::mat4*>(uploadMem)[4] = invViewMatrix;
	reinterpret_cast<glm::mat4*>(uploadMem)[5] = invProjectionMatrix;
	*reinterpret_cast<glm::vec3*>(uploadMem + 384) = cameraPosition;
	*reinterpret_cast<float*>(uploadMem + 396) = gameTime;
	uploadBuffer.Flush();

	eg::DC.CopyBuffer(uploadBuffer.buffer, m_buffer, uploadBuffer.offset, 0, BUFFER_SIZE);

	m_buffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Vertex | eg::ShaderAccessFlags::Fragment);
}
