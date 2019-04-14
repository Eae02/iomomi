#pragma once

class GravitySwitchMaterial : public eg::IMaterial
{
public:
	size_t PipelineHash() const override;
	
	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;

	static GravitySwitchMaterial instance;
};
