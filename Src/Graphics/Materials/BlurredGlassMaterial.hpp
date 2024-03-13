#pragma once

class BlurredGlassMaterial : public eg::IMaterial
{
public:
	BlurredGlassMaterial(eg::ColorLin color, bool isBlurry);

	size_t PipelineHash() const override;

	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;

	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;

	virtual OrderRequirement GetOrderRequirement() const override { return OrderRequirement::OnlyOrdered; }

	VertexInputConfiguration GetVertexInputConfiguration(const void* drawArgs) const override;

private:
	std::optional<bool> ShouldRenderBlurry(const struct MeshDrawArgs& meshDrawArgs) const;

	eg::Buffer m_parametersBuffer;
	eg::DescriptorSet m_descriptorSet;

	bool m_isBlurry;
};
