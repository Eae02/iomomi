#pragma once

class GravityCornerMaterial : public eg::IMaterial
{
public:
	GravityCornerMaterial() = default;
	
	size_t PipelineHash() const override;
	
	void BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	void BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	static GravityCornerMaterial instance;
};
