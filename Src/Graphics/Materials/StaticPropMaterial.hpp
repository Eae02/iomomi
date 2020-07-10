#pragma once

#include "../QualityLevel.hpp"

class StaticPropMaterial : public eg::IMaterial
{
public:
	struct InstanceData
	{
		glm::mat3x4 transform;
		glm::vec2 textureScale;
		
		InstanceData(const glm::mat4& _transform, glm::vec2 _textureScale = glm::vec2(1))
			: transform(glm::transpose(_transform)), textureScale(_textureScale) { }
	};
	
	struct FlagsPipelinePushConstantData
	{
		glm::mat4 viewProj;
		uint32_t flags;
	};
	
	size_t PipelineHash() const override;
	
	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	virtual OrderRequirement GetOrderRequirement() const override { return OrderRequirement::OnlyUnordered; }
	
	static void InitializeForCommon3DVS(eg::GraphicsPipelineCreateInfo& pipelineCI);
	
	static void InitAssetTypes();
	
	static bool AssetLoader(const eg::AssetLoadContext& loadContext);
	
	static eg::PipelineRef FlagsPipelineBackCull;
	static eg::PipelineRef FlagsPipelineNoCull;
	
private:
	const eg::Texture* m_albedoTexture;
	const eg::Texture* m_normalMapTexture;
	const eg::Texture* m_miscMapTexture;
	mutable eg::DescriptorSet m_descriptorSet;
	mutable eg::DescriptorSet m_descriptorSetEditor;
	mutable bool m_descriptorsInitialized = false;
	float m_roughnessMin;
	float m_roughnessMax;
	glm::vec2 m_textureScale;
	bool m_backfaceCull = true;
	bool m_castShadows = true;
	QualityLevel m_minShadowQuality = QualityLevel::Medium;
	uint32_t m_objectFlags = 0;
};
