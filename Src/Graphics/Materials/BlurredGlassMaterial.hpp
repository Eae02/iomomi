#pragma once

class BlurredGlassMaterial : public eg::IMaterial
{
public:
	size_t PipelineHash() const override;
	
	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	virtual OrderRequirement GetOrderRequirement() const override { return OrderRequirement::OnlyOrdered; }
	
	bool isBlurry = false;
	
	eg::ColorLin color;
	
private:
	std::optional<bool> ShouldRenderBlurry(const struct MeshDrawArgs& meshDrawArgs) const;
};
