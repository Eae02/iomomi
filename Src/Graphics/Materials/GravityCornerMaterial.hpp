#pragma once

class GravityCornerMaterial : public eg::IMaterial
{
public:
	GravityCornerMaterial() = default;
	
	size_t PipelineHash() const override;
	
	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	static GravityCornerMaterial instance;
};
