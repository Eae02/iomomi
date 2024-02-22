#pragma once

class GravityGunRenderer
{
public:
	explicit GravityGunRenderer(const eg::Model& model);

	void Draw(
		std::span<const glm::mat4> meshTransforms, float glowIntensityBoost, class RenderTexManager& renderTexManager);

private:
	eg::Pipeline m_mainMaterialPipeline;
	eg::Pipeline m_glowMaterialPipeline;

	eg::DescriptorSet m_mainMaterialDescriptorSet;
	eg::DescriptorSet m_glowMaterialDescriptorSet;

	const eg::Model* m_model;
	int m_mainMaterialIndex;
	int m_glowMaterialIndex;
};
