#include "BlurRenderer.hpp"

#include "../Utils.hpp"
#include "GraphicsCommon.hpp"

static std::vector<std::pair<eg::Format, eg::Pipeline>> blurPipelines;

static eg::PipelineRef GetBlurPipeline(eg::Format format)
{
	for (auto& [pipelineFormat, pipeline] : blurPipelines)
	{
		if (pipelineFormat == format)
			return pipeline;
	}

	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").ToStageInfo();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Blur.fs.glsl").ToStageInfo();
	pipelineCI.label = "BlurPipeline";
	pipelineCI.colorAttachmentFormats[0] = format;
	eg::Pipeline pipeline = eg::Pipeline::Create(pipelineCI);
	eg::PipelineRef pipelineRef = pipeline;
	blurPipelines.emplace_back(format, std::move(pipeline));

	return pipelineRef;
}

static eg::DescriptorSet descriptorSet0;

static void OnShutdown()
{
	blurPipelines.clear();
	descriptorSet0.Destroy();
}

EG_ON_SHUTDOWN(OnShutdown)

BlurRenderer::BlurRenderer(uint32_t blurLevels, eg::Format format)
	: m_levels(blurLevels), m_format(format), m_blurPipeline(GetBlurPipeline(format)), m_framebuffersTmp(blurLevels),
	  m_framebuffersOut(blurLevels)
{
	if (descriptorSet0.handle == nullptr)
	{
		descriptorSet0 = eg::DescriptorSet(m_blurPipeline, 0);
		descriptorSet0.BindSampler(samplers::linearClamp, 0);
		descriptorSet0.BindUniformBuffer(frameDataUniformBuffer, 1, eg::BIND_BUFFER_OFFSET_DYNAMIC, sizeof(float) * 5);
	}

	m_blurXDescriptorSets.resize(blurLevels);
	m_blurYDescriptorSets.resize(blurLevels);
}

void BlurRenderer::MaybeUpdateResolution(uint32_t newWidth, uint32_t newHeight)
{
	if (newWidth == m_inputWidth && newHeight == m_inputHeight)
		return;
	m_inputWidth = newWidth;
	m_inputHeight = newHeight;

	m_blurTextureOut.Destroy();
	m_blurTextureTmp.Destroy();
	for (uint32_t i = 0; i < m_levels; i++)
	{
		m_framebuffersOut[i].Destroy();
		m_framebuffersTmp[i].Destroy();
	}

	eg::TextureCreateInfo textureCI;
	textureCI.width = newWidth / 2;
	textureCI.height = newHeight / 2;
	textureCI.mipLevels = m_levels;
	textureCI.flags =
		eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample | eg::TextureFlags::ManualBarrier;
	textureCI.format = m_format;
	textureCI.label = "GlassBlurDst";

	m_blurTextureTmp = eg::Texture::Create2D(textureCI);
	m_blurTextureOut = eg::Texture::Create2D(textureCI);

	for (uint32_t i = 0; i < m_levels; i++)
	{
		eg::FramebufferAttachment colorAttachment;
		colorAttachment.subresource.mipLevel = i;

		colorAttachment.texture = m_blurTextureTmp.handle;
		m_framebuffersTmp[i] = eg::Framebuffer({ &colorAttachment, 1 });

		colorAttachment.texture = m_blurTextureOut.handle;
		m_framebuffersOut[i] = eg::Framebuffer({ &colorAttachment, 1 });

		m_blurXDescriptorSets[i] = eg::DescriptorSet(m_blurPipeline, 1);
		m_blurXDescriptorSets[i].BindTextureView(
			m_blurTextureOut.GetView(eg::TextureSubresource{ .firstMipLevel = i, .numMipLevels = 1 }), 0
		);

		m_blurYDescriptorSets[i] = eg::DescriptorSet(m_blurPipeline, 1);
		m_blurYDescriptorSets[i].BindTextureView(
			m_blurTextureTmp.GetView(eg::TextureSubresource{ .firstMipLevel = i, .numMipLevels = 1 }), 0
		);
	}
}

void BlurRenderer::DoBlurPass(
	const glm::vec2& blurVector, const glm::vec2& sampleOffset, eg::DescriptorSetRef inputTextureDescriptorSet,
	int inputLod, eg::FramebufferRef dstFramebuffer
) const
{
	const float pc[] = { blurVector.x, blurVector.y, sampleOffset.x, sampleOffset.y, static_cast<float>(inputLod) };
	uint32_t frameUniformDataOffset = PushFrameUniformData(ToCharSpan<float>(pc));

	eg::RenderPassBeginInfo rp1BeginInfo;
	rp1BeginInfo.framebuffer = dstFramebuffer.handle;
	rp1BeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	eg::DC.BeginRenderPass(rp1BeginInfo);

	eg::DC.BindPipeline(m_blurPipeline);

	eg::DC.BindDescriptorSet(descriptorSet0, 0, { &frameUniformDataOffset, 1 });
	eg::DC.BindDescriptorSet(inputTextureDescriptorSet, 1);

	eg::DC.Draw(0, 3, 0, 1);

	eg::DC.EndRenderPass();
}

static inline void ChangeUsageForShaderSample(eg::TextureRef texture, int mipLevel)
{
	eg::TextureBarrier barrier;
	barrier.oldAccess = eg::ShaderAccessFlags::None;
	barrier.newAccess = eg::ShaderAccessFlags::Fragment;
	barrier.oldUsage = eg::TextureUsage::FramebufferAttachment;
	barrier.newUsage = eg::TextureUsage::ShaderSample;
	barrier.subresource.firstMipLevel = mipLevel;
	barrier.subresource.numMipLevels = 1;
	eg::DC.Barrier(texture, barrier);
}

void BlurRenderer::Render(eg::DescriptorSetRef inputTextureDescriptorSet, float blurScale) const
{
	glm::vec2 pixelSize = blurScale / glm::vec2(m_inputWidth, m_inputHeight);

	for (int i = 0; i < static_cast<int>(m_levels); i++)
	{
		DoBlurPass(
			glm::vec2(2 * pixelSize.x, 0), pixelSize / 2.0f,
			i != 0 ? eg::DescriptorSetRef(m_blurXDescriptorSets[i - 1]) : inputTextureDescriptorSet, std::max(i - 1, 0),
			m_framebuffersTmp[i]
		);
		ChangeUsageForShaderSample(m_blurTextureTmp, i);

		pixelSize *= 2.0f;

		DoBlurPass(glm::vec2(0, 2 * pixelSize.y), -pixelSize / 2.0f, m_blurYDescriptorSets[i], i, m_framebuffersOut[i]);
		ChangeUsageForShaderSample(m_blurTextureOut, i);
	}
}
