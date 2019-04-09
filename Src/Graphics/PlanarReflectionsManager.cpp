#include "PlanarReflectionsManager.hpp"

PlanarReflectionsManager::PlanarReflectionsManager()
{
	SetQuality(QualityLevel::Medium);
}

void PlanarReflectionsManager::BeginFrame()
{
	m_numUsedReflectionTextures = 0;
}

uint32_t PlanarReflectionsManager::RenderPlanarReflections(const eg::Plane& plane,
	const RenderSettings& realRenderSettings, const PlanarReflectionsManager::RenderCallback& renderCallback)
{
	if (m_depthTexture.handle == nullptr)
	{
		eg::Texture2DCreateInfo depthTextureCI;
		depthTextureCI.format = eg::Format::Depth16;
		depthTextureCI.width = m_textureWidth;
		depthTextureCI.height = m_textureHeight;
		depthTextureCI.mipLevels = 1;
		depthTextureCI.flags = eg::TextureFlags::FramebufferAttachment;
		
		m_depthTexture = eg::Texture::Create2D(depthTextureCI);
	}
	
	if (m_numUsedReflectionTextures >= m_reflectionTextures.size())
	{
		eg::Texture2DCreateInfo textureCI;
		textureCI.format = m_textureFormat;
		textureCI.width = m_textureWidth;
		textureCI.height = m_textureHeight;
		textureCI.mipLevels = 1;
		textureCI.flags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample;
		
		eg::Texture texture = eg::Texture::Create2D(textureCI);
		
		eg::FramebufferAttachment colorAttachment(texture.handle);
		eg::FramebufferAttachment depthAttachment(m_depthTexture.handle);
		eg::Framebuffer framebuffer({ &colorAttachment, 1 }, depthAttachment);
		
		m_reflectionTextures.push_back({ std::move(texture), std::move(framebuffer) });
	}
	
	const uint32_t id = m_numUsedReflectionTextures;
	
	float cameraDistToPlane = glm::dot(plane.GetNormal(), realRenderSettings.cameraPosition);
	m_renderSettings.cameraPosition = realRenderSettings.cameraPosition - (2 * cameraDistToPlane) * plane.GetNormal();
	
	glm::vec3 flipU;
	if (std::abs(plane.GetNormal().y) < 0.999f)
		flipU = glm::cross(plane.GetNormal(), glm::vec3(0, 1, 0));
	else
		flipU = glm::vec3(0, 1, 0);
	
	glm::mat3 flipRotation(flipU, glm::cross(flipU, plane.GetNormal()), plane.GetNormal());
	
	m_renderSettings.viewProjection =
		realRenderSettings.viewProjection *
		glm::mat4(flipRotation) *
		glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, plane.GetDistance())) *
		glm::scale(glm::mat4(1.0f), glm::vec3(0, 0, -1)) *
		glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -plane.GetDistance())) *
		glm::mat4(glm::transpose(flipRotation));
	
	m_renderSettings.invViewProjection = 
		glm::mat4(glm::transpose(flipRotation)) *
		glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -plane.GetDistance())) *
		glm::scale(glm::mat4(1.0f), glm::vec3(0, 0, -1)) *
		glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, plane.GetDistance())) *
		glm::mat4(flipRotation) * 
		realRenderSettings.invViewProjection;
	
	m_renderSettings.gameTime = realRenderSettings.gameTime;
	m_renderSettings.UpdateBuffer();
	
	renderCallback(m_renderSettings, m_reflectionTextures[id].framebuffer);
	
	m_numUsedReflectionTextures++;
	return id;
}

void PlanarReflectionsManager::SetQuality(QualityLevel qualityLevel)
{
	m_qualityLevel = qualityLevel;
	m_textureFormat = eg::Format::R8G8B8A8_UNorm;
	ResolutionChanged();
}

void PlanarReflectionsManager::ResolutionChanged()
{
	m_textureWidth = eg::CurrentResolutionX() / 2;
	m_textureHeight = eg::CurrentResolutionY() / 2;
	m_reflectionTextures.clear();
	m_depthTexture = { };
}
