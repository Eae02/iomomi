#pragma once

class StaticPropMaterial : public eg::IMaterial
{
public:
	size_t PipelineHash() const override;
	
	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	static void InitAssetTypes();
	
	static bool AssetLoader(const eg::AssetLoadContext& loadContext);
	
private:
	const eg::Texture* m_albedoTexture;
	const eg::Texture* m_normalMapTexture;
	const eg::Texture* m_miscMapTexture;
	mutable eg::DescriptorSet m_descriptorSetGame;
	mutable eg::DescriptorSet m_descriptorSetEditor;
	mutable eg::DescriptorSet m_descriptorSetPlanarRefl;
	mutable bool m_descriptorsInitialized = false;
	float m_roughnessMin;
	float m_roughnessMax;
	glm::vec2 m_textureScale;
	bool m_backfaceCull = true;
	bool m_castShadows = true;
};
