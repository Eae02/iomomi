#pragma once

#include "StaticPropMaterial.hpp"

class ScreenMaterial : public eg::IMaterial
{
public:
	using InstanceData = StaticPropMaterial::InstanceData;

	ScreenMaterial(int resX, int resY);

	size_t PipelineHash() const override;

	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;

	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;

	void RenderTexture(const eg::ColorLin& clearColor);

	std::function<void(eg::SpriteBatch&)> render;

	int ResX() const { return m_resX; }
	int ResY() const { return m_resY; }

private:
	int m_resX, m_resY;
	eg::Texture m_texture;
	eg::Framebuffer m_framebuffer;
	eg::DescriptorSet m_descriptorSet;
	eg::SpriteBatch m_spriteBatch;
};
