#pragma once

#include "../QualityLevel.hpp"
#include "MeshDrawArgs.hpp"

class StaticPropMaterial : public eg::IMaterial
{
public:
	struct InstanceData
	{
		glm::mat3x4 transform;
		glm::vec4 textureRange;

		InstanceData(const glm::mat4& _transform) : transform(glm::transpose(_transform)), textureRange(0, 0, 1, 1) {}

		InstanceData(const glm::mat4& _transform, glm::vec2 textureScale)
			: transform(glm::transpose(_transform)), textureRange(0, 0, textureScale.x, textureScale.y)
		{
		}

		InstanceData(const glm::mat4& _transform, glm::vec2 textureMin, glm::vec2 textureMax)
			: transform(glm::transpose(_transform)),
			  textureRange(textureMin.x, textureMin.y, textureMax.x, textureMax.y)
		{
		}
	};

	struct FlagsPipelinePushConstantData
	{
		glm::mat4 viewProj;
		uint32_t flags;
	};

	size_t PipelineHash() const override;

	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;

	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;

	OrderRequirement GetOrderRequirement() const override { return OrderRequirement::OnlyUnordered; }

	VertexInputConfiguration GetVertexInputConfiguration(const void* drawArgs) const override;

	bool CheckInstanceDataType(const std::type_info* instanceDataType) const override;

	const eg::Texture& AlbedoTexture() const { return *m_albedoTexture; }
	glm::vec2 TextureScale() const { return m_textureScale; }

	static const StaticPropMaterial& GetFromWallMaterial(uint32_t index);

	static void InitializeForCommon3DVS(eg::GraphicsPipelineCreateInfo& pipelineCI);
	static void InitializeInstanceDataVertexInput(eg::GraphicsPipelineCreateInfo& pipelineCI, uint32_t firstAttribute);

	static void LazyInitGlobals();

	static VertexInputConfiguration GetVertexInputConfiguration(bool alphaTest, MeshDrawMode drawMode);

private:
	friend bool StaticPropMaterialAssetLoader(const eg::AssetLoadContext& loadContext);

	void CreateDescriptorSetAndParamsBuffer();

	eg::PipelineRef GetPipeline(MeshDrawMode drawMode) const;

	const eg::Texture* m_albedoTexture;
	const eg::Texture* m_normalMapTexture;
	const eg::Texture* m_miscMapTexture;
	std::optional<uint32_t> m_textureLayer;
	eg::Buffer m_parametersBuffer;
	eg::DescriptorSet m_descriptorSet;
	eg::DescriptorSet m_plShadowDescriptorSet;
	float m_roughnessMin;
	float m_roughnessMax;
	glm::vec2 m_textureScale;
	bool m_backfaceCull = true;
	bool m_backfaceCullEditor = false;
	bool m_castShadows = true;
	bool m_alphaTest = false;
	QualityLevel m_minShadowQuality = QualityLevel::Medium;
};
