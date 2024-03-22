#pragma once

class GravityGunRenderer
{
public:
	explicit GravityGunRenderer(const eg::Model& model);

	void Draw(
		std::span<const glm::mat4> meshTransforms, float glowIntensityBoost, const struct RenderTargets& renderTargets
	);

private:
	eg::Pipeline m_mainMaterialPipeline;
	eg::Pipeline m_glowMaterialPipeline;

	eg::DescriptorSet m_mainMaterialDescriptorSet;
	eg::DescriptorSet m_glowMaterialDescriptorSet;

	eg::Buffer m_parametersBuffer;

	uint32_t m_meshTransformsStride;
	uint32_t m_meshTransformsBufferSize = 0;
	eg::Buffer m_meshTransformsBuffer;
	eg::DescriptorSet m_meshTransformsDescriptorSet;

	uint32_t m_renderTargetsUID = 0;
	eg::Texture m_depthAttachment;
	eg::Framebuffer m_framebuffer;

	const eg::Model* m_model;
	int m_mainMaterialIndex;
	int m_glowMaterialIndex;
};
