#pragma once

#include "ObjectMaterial.hpp"

class StaticPropMaterial : public ObjectMaterial
{
public:
	eg::PipelineRef GetPipeline(PipelineType pipelineType) const override;
	
	void Bind(ObjectMaterial::PipelineType boundPipeline) const override;
	
	static void InitAssetTypes();
	
	static bool AssetLoader(const eg::AssetLoadContext& loadContext);
	
private:
	const eg::Texture* m_albedoTexture;
	const eg::Texture* m_normalMapTexture;
	const eg::Texture* m_miscMapTexture;
	float m_roughnessMin;
	float m_roughnessMax;
};
