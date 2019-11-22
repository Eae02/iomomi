#pragma once

#include "../PlanarReflectionsManager.hpp"
#include "../RenderSettings.hpp"
#include "../Materials/MeshDrawArgs.hpp"
#include "../DeferredRenderer.hpp"

class GooPlaneMaterial : public eg::IMaterial
{
public:
	size_t PipelineHash() const override;
	
	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	static void OnInit();
	static void OnShutdown();
	
	ReflectionPlane m_reflectionPlane;
	
private:
	static eg::Pipeline s_pipelineReflEnabled;
	static eg::Pipeline s_pipelineReflDisabled;
	static eg::Pipeline s_emissivePipeline;
	static eg::Buffer s_textureTransformsBuffer;
	static eg::DescriptorSet s_descriptorSet;
};
