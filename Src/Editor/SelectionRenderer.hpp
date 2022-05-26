#pragma once

class SelectionRenderer
{
public:
	SelectionRenderer() = default;
	
	void Draw(float intensity, const glm::mat4& transform, const eg::Model& model, int meshIndex);
	void EndDraw();
	
	void BeginFrame(uint32_t resX, uint32_t resY);
	void EndFrame();
	
	glm::mat4 viewProjection;
	
private:
	eg::Texture m_texture;
	bool m_hasRendered = false;
	eg::Framebuffer m_framebuffer;
};
