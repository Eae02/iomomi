#pragma once

class DecalMaterial : public eg::IMaterial
{
public:
	struct InstanceData
	{
		glm::mat3x4 transform;
		glm::vec2 textureMin;
		glm::vec2 textureMax;
		
		InstanceData(const glm::mat4& _transform, glm::vec2 _textureMin = glm::vec2(0), glm::vec2 _textureMax = glm::vec2(1))
			: transform(glm::transpose(_transform)), textureMin(_textureMin), textureMax(_textureMax) { }
	};
	
	DecalMaterial(const eg::Texture& albedoTexture, const eg::Texture& normalMapTexture);
	
	size_t PipelineHash() const override;
	
	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	bool CheckInstanceDataType(const std::type_info* instanceDataType) const override;
	
	float AspectRatio() const
	{
		return m_aspectRatio;
	}
	
	eg::TextureRef AlbedoTexture() const
	{
		return m_albedoTexture;
	}
	
	static eg::MeshBatch::Mesh GetMesh();
	
	float m_opacity = 1;
	float m_roughness = 1;
	bool m_inheritNormals = false;
	
private:
	eg::TextureRef m_albedoTexture;
	eg::TextureRef m_normalMapTexture;
	
	float m_aspectRatio;
	
	mutable eg::DescriptorSet m_descriptorSet;
	mutable bool m_descriptorSetInitialized = false;
};
