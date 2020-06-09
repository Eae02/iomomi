#pragma once

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
	
private:
	static eg::Pipeline s_pipeline;
	static eg::Buffer s_textureTransformsBuffer;
	static eg::DescriptorSet s_descriptorSet;
};
