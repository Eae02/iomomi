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
	createInfo.size = sizeof(Data);
	createInfo.label = "RenderSettings";
	m_buffer = eg::Buffer(createInfo);

	m_vertexShaderDescriptorSet.BindUniformBuffer(m_buffer, 0);
}

void RenderSettings::UpdateBuffer()
{
	eg::UploadBuffer uploadBuffer = eg::GetTemporaryUploadBuffer(sizeof(Data));
	char* uploadMem = reinterpret_cast<char*>(uploadBuffer.Map());
	std::memcpy(uploadMem, &data, sizeof(Data));
	uploadBuffer.Flush();

	eg::DC.CopyBuffer(uploadBuffer.buffer, m_buffer, uploadBuffer.offset, 0, sizeof(Data));

	m_buffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Vertex | eg::ShaderAccessFlags::Fragment);
}
