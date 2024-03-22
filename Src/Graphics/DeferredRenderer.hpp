#pragma once

#include "../Water/WaterRenderer.hpp"
#include "Lighting/PointLight.hpp"

class DeferredRenderer
{
public:
	DeferredRenderer() = default;

	void RenderTargetsCreated(const struct RenderTargets& renderTargets);

	void SetSSAOTexture(std::optional<eg::TextureRef> ssaoTexture);

	void BeginGeometry() const;

	void BeginLighting(const struct RenderTargets& renderTargets);

	void DrawPointLights(
		const std::vector<std::shared_ptr<PointLight>>& pointLights, const struct RenderTargets& renderTargets,
		uint32_t shadowResolution
	) const;

	static void CreatePipelines();
	static void DestroyPipelines();

private:
	eg::Framebuffer m_geometryPassFramebuffer;

	eg::DescriptorSet m_ambientDS0;

	eg::DescriptorSet m_pointLightDS0;
	eg::DescriptorSet m_pointLightParametersDS;

	eg::DescriptorSet m_frameParametersDescriptorSet;

	std::optional<eg::DescriptorSet> m_ssaoTextureDescriptorSet;

	static eg::Pipeline m_ambientPipelineWithSSAO;
	static eg::Pipeline m_ambientPipelineWithoutSSAO;
	static eg::Pipeline m_pointLightPipelines[2][2]; // indices are [soft shadow enable][water enable]
};
