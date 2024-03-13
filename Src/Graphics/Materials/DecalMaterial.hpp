#pragma once

class DecalMaterial : public eg::IMaterial
{
public:
	struct InstanceData
	{
		glm::mat3x4 transform;
		glm::vec2 textureMin;
		glm::vec2 textureMax;

		InstanceData(
			const glm::mat4& _transform, glm::vec2 _textureMin = glm::vec2(0), glm::vec2 _textureMax = glm::vec2(1)
		)
			: transform(glm::transpose(_transform)), textureMin(_textureMin), textureMax(_textureMax)
		{
		}
	};

	struct Parameters
	{
		float opacity = 1;
		float roughness = 1;
		bool inheritNormals = false;
	};

	DecalMaterial(const eg::Texture& albedoTexture, const eg::Texture& normalMapTexture, const Parameters& parameters);

	size_t PipelineHash() const override;
	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;

	bool CheckInstanceDataType(const std::type_info* instanceDataType) const override;

	VertexInputConfiguration GetVertexInputConfiguration(const void* drawArgs) const override;

	float AspectRatio() const { return m_aspectRatio; }
	eg::TextureRef AlbedoTexture() const { return m_albedoTexture; }
	eg::TextureRef NormalMapTexture() const { return m_normalMapTexture; }

	static eg::MeshBatch::Mesh GetMesh();

	static void LazyInitGlobals();

	static constexpr uint32_t POSITION_BINDING = 0;
	static constexpr uint32_t INSTANCE_DATA_BINDING = 1;

private:
	eg::Buffer m_parametersBuffer;
	eg::TextureRef m_albedoTexture;
	eg::TextureRef m_normalMapTexture;

	eg::DescriptorSet m_descriptorSet;

	bool m_inheritNormals;
	float m_aspectRatio;
};
