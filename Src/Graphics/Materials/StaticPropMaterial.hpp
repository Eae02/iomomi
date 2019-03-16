#pragma once

class StaticPropMaterial : public eg::IMaterial
{
public:
	size_t PipelineHash() const override;
	
	void BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	void BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	static void InitAssetTypes();
	
	static bool AssetLoader(const eg::AssetLoadContext& loadContext);
	
private:
	const eg::Texture* m_albedoTexture;
	const eg::Texture* m_normalMapTexture;
	const eg::Texture* m_miscMapTexture;
	mutable eg::DescriptorSet m_descriptorSetGame;
	mutable eg::DescriptorSet m_descriptorSetEditor;
	mutable bool m_descriptorsInitialized = false;
	float m_roughnessMin;
	float m_roughnessMax;
	glm::vec2 m_textureScale;
};
