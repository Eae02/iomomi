#pragma once

class RenderSettings
{
public:
	RenderSettings();
	
	void UpdateBuffer();
	
	eg::BufferRef Buffer() const
	{
		return m_buffer;
	}
	
	glm::mat4 viewProjection;
	glm::mat4 invViewProjection;
	glm::mat4 viewMatrix;
	glm::mat4 projectionMatrix;
	glm::mat4 invViewMatrix;
	glm::mat4 invProjectionMatrix;
	glm::vec3 cameraPosition;
	float gameTime;
	
	static constexpr uint32_t BUFFER_SIZE = sizeof(glm::mat4) * 6 + sizeof(float) * 4;
	
	static RenderSettings* instance;
	
	void BindVertexShaderDescriptorSet(uint32_t set = 0) const
	{
		eg::DC.BindDescriptorSet(m_vertexShaderDescriptorSet, set);
	}
	
private:
	eg::DescriptorSet m_vertexShaderDescriptorSet;
	eg::Buffer m_buffer;
};

inline float DepthDrawOrder(const glm::vec3& pos)
{
	return -glm::distance2(RenderSettings::instance->cameraPosition, pos);
}
