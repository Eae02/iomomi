#pragma once

class DecalMaterial : public eg::IMaterial
{
public:
	DecalMaterial(const eg::Texture& albedoTexture, const eg::Texture& normalMapTexture);
	
	size_t PipelineHash() const override;
	
	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	float AspectRatio() const
	{
		return m_aspectRatio;
	}
	
	static void InitAssetTypes();
	
	static bool AssetLoader(const eg::AssetLoadContext& loadContext);
	
	static eg::MeshBatch::Mesh GetMesh();
	
private:
	eg::TextureRef m_albedoTexture;
	eg::TextureRef m_normalMapTexture;
	
	float m_aspectRatio;
	
	float m_opacity;
	float m_roughness;
	
	mutable eg::DescriptorSet m_descriptorSet;
	mutable bool m_descriptorSetInitialized = false;
};
