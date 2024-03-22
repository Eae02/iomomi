#pragma once

#include "QualityLevel.hpp"

class SSR
{
public:
	explicit SSR(QualityLevel qualityLevel);

	struct SSRRenderArgs
	{
		eg::ColorLin fallbackColor;
		float intensity;
		const struct RenderTargets* renderTargets;
		std::function<void()> renderAdditionalCallback;
	};

	void RenderTargetsCreated(const struct RenderTargets& renderTargets);

	void Render(const SSRRenderArgs& renderArgs);

	QualityLevel GetQualityLevel() const { return m_qualityLevel; }

	static const float MAX_DISTANCE;

	static constexpr eg::Format COLOR_FORMAT = eg::Format::R16G16B16A16_Float;
	static constexpr eg::Format DEPTH_FORMAT = eg::Format::Depth16;

private:
	void CreatePipeline();

	QualityLevel m_qualityLevel;

	eg::Pipeline m_pipelineInitial;
	eg::Pipeline m_pipelineBlendPass;
	eg::Pipeline m_blur1Pipeline;
	eg::Pipeline m_blur2Pipeline;

	eg::Texture m_attachmentTemp1;
	eg::Texture m_attachmentTemp2;
	eg::Texture m_attachmentDepth;

	eg::Framebuffer m_framebufferTemp1;
	eg::Framebuffer m_framebufferTemp2;

	eg::Buffer m_blurParametersBuffer;
	eg::DescriptorSet m_blurParametersBufferDS;

	eg::DescriptorSet m_initialPassBuffersDS;

	eg::DescriptorSet m_initialPassAttachmentsDS;
	eg::DescriptorSet m_blendPassAttachmentsDS;

	eg::DescriptorSet m_blurPass1AttachmentsDS;
	eg::DescriptorSet m_blurPass2AttachmentsDS;
};
