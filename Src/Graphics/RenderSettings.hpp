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
	glm::vec3 cameraPosition;
	float gameTime;
	
	static constexpr uint32_t BUFFER_SIZE = sizeof(glm::mat4) + sizeof(glm::mat4) + sizeof(float) * 4;
	
	static RenderSettings* instance;
	
private:
	eg::Buffer m_buffer;
};
