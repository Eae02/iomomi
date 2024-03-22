#include "SSR.hpp"

#include "../Utils.hpp"
#include "GraphicsCommon.hpp"
#include "RenderSettings.hpp"
#include "RenderTargets.hpp"

const float SSR::MAX_DISTANCE = 20;

static eg::DescriptorSetBinding buffersDescriptorSetBindings[] = {
	eg::DescriptorSetBinding{
		.binding = 0,
		.type = eg::BindingTypeUniformBuffer{},
		.shaderAccess = eg::ShaderAccessFlags::Fragment,
	},
	eg::DescriptorSetBinding{
		.binding = 1,
		.type = eg::BindingTypeUniformBuffer{ .dynamicOffset = true },
		.shaderAccess = eg::ShaderAccessFlags::Fragment,
	},
};

struct BlurParameters
{
	float blurDistance;
	float maxBlurDist;
	uint32_t _padding[2];
	float blurCoeff[16];
};

static float* ssrBlurIntensity = eg::TweakVarFloat("ssr_blur_scale", 0.05f, 0.0f);
static float* ssrBlurFalloff = eg::TweakVarFloat("ssr_blur_fo", 5.0f, 0.0f);
static float* ssrBlurMax = eg::TweakVarFloat("ssr_blur_max", 4.0f, 0.0f);

BlurParameters InitializeBlurParameters(QualityLevel qualityLevel)
{
	const uint32_t blurRadius = qvar::ssrBlurRadius(qualityLevel);

	BlurParameters parameters = {};
	float coeffSum = 1;
	parameters.blurCoeff[0] = 1;
	for (uint32_t i = 1; i < blurRadius; i++)
	{
		float r = (static_cast<float>(i) * *ssrBlurFalloff) / static_cast<float>(blurRadius - 1);
		parameters.blurCoeff[i] = std::exp(-r * r);
		coeffSum += 2 * parameters.blurCoeff[i];
	}
	for (uint32_t i = 0; i < blurRadius; i++)
	{
		parameters.blurCoeff[i] /= coeffSum;
	}

	const float relativeRadius = (float)qvar::ssrBlurRadius(QualityLevel::VeryHigh) / static_cast<float>(blurRadius);
	parameters.blurDistance = *ssrBlurIntensity * SSR::MAX_DISTANCE / (float)qvar::ssrBlurRadius(qualityLevel);
	parameters.maxBlurDist = *ssrBlurMax * relativeRadius;

	return parameters;
}

SSR::SSR(QualityLevel qualityLevel) : m_qualityLevel(qualityLevel)
{
	const eg::ShaderModuleAsset& ssrBlurShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/SSRBlur.fs.glsl");
	eg::ShaderStageInfo postVertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").ToStageInfo();

	uint32_t linearSamples = qvar::ssrLinearSamples(qualityLevel);
	uint32_t binarySamples = qvar::ssrBinSearchSamples(qualityLevel);

	const eg::SpecializationConstantEntry specConstants[] = {
		{ 0, linearSamples },
		{ 1, binarySamples },
		{ 2, MAX_DISTANCE },
	};

	eg::GraphicsPipelineCreateInfo pipeline1CI;
	pipeline1CI.vertexShader = postVertexShader;
	pipeline1CI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/SSR.fs.glsl").ToStageInfo();
	pipeline1CI.fragmentShader.specConstants = specConstants;
	pipeline1CI.enableDepthWrite = true;
	pipeline1CI.enableDepthTest = true;
	pipeline1CI.depthCompare = eg::CompareOp::Always;
	pipeline1CI.colorAttachmentFormats[0] = SSR::COLOR_FORMAT;
	pipeline1CI.depthAttachmentFormat = SSR::DEPTH_FORMAT;
	pipeline1CI.descriptorSetBindings[0] = buffersDescriptorSetBindings;
	pipeline1CI.label = "SSR[Initial]";
	m_pipelineInitial = eg::Pipeline::Create(pipeline1CI);

	eg::GraphicsPipelineCreateInfo pipelineBlendPassCI;
	pipelineBlendPassCI.vertexShader = postVertexShader;
	pipelineBlendPassCI.fragmentShader =
		eg::GetAsset<eg::ShaderModuleAsset>("Shaders/SSRBlendPass.fs.glsl").ToStageInfo();
	pipelineBlendPassCI.enableDepthWrite = false;
	pipelineBlendPassCI.enableDepthTest = false;
	pipelineBlendPassCI.blendStates[0] = eg::BlendState(
		eg::BlendFunc::Add, eg::BlendFunc::Add, eg::BlendFactor::DstColor, eg::BlendFactor::DstAlpha,
		eg::BlendFactor::Zero, eg::BlendFactor::Zero
	);
	pipelineBlendPassCI.colorAttachmentFormats[0] = SSR::COLOR_FORMAT;
	pipelineBlendPassCI.depthAttachmentFormat = SSR::DEPTH_FORMAT;
	pipelineBlendPassCI.descriptorSetBindings[0] = buffersDescriptorSetBindings;
	pipelineBlendPassCI.label = "SSR[Blend]";
	m_pipelineBlendPass = eg::Pipeline::Create(pipelineBlendPassCI);

	const uint32_t blurRadius = qvar::ssrBlurRadius(qualityLevel);
	const eg::SpecializationConstantEntry specConstantsBlur[] = { { 0, blurRadius } };

	eg::GraphicsPipelineCreateInfo pipelineBlurCI;
	pipelineBlurCI.vertexShader = postVertexShader;
	pipelineBlurCI.fragmentShader = {
		.shaderModule = ssrBlurShader.GetVariant("VPass1"),
		.specConstants = specConstantsBlur,
	};
	pipelineBlurCI.colorAttachmentFormats[0] = SSR::COLOR_FORMAT;
	pipelineBlurCI.label = "SSR[Blur1]";
	m_blur1Pipeline = eg::Pipeline::Create(pipelineBlurCI);

	pipelineBlurCI.fragmentShader.shaderModule = ssrBlurShader.GetVariant("VPass2");
	pipelineBlurCI.colorAttachmentFormats[0] = lightColorAttachmentFormat;
	pipelineBlurCI.depthAttachmentFormat = GB_DEPTH_FORMAT;
	pipelineBlurCI.depthStencilUsage = eg::TextureUsage::DepthStencilReadOnly;
	pipelineBlurCI.label = "SSR[Blur2]";
	m_blur2Pipeline = eg::Pipeline::Create(pipelineBlurCI);

	m_initialPassBuffersDS = eg::DescriptorSet(buffersDescriptorSetBindings);
	m_initialPassBuffersDS.BindUniformBuffer(RenderSettings::instance->Buffer(), 0);
	m_initialPassBuffersDS.BindUniformBuffer(
		frameDataUniformBuffer, 1, eg::BIND_BUFFER_OFFSET_DYNAMIC, sizeof(float) * 4
	);

	BlurParameters blurParameters = InitializeBlurParameters(qualityLevel);

	m_blurParametersBuffer = eg::Buffer(eg::BufferFlags::UniformBuffer, sizeof(BlurParameters), &blurParameters);
	m_blurParametersBufferDS = eg::DescriptorSet(m_blur1Pipeline, 0);
	m_blurParametersBufferDS.BindUniformBuffer(m_blurParametersBuffer, 0);
}

void SSR::RenderTargetsCreated(const RenderTargets& renderTargets)
{
	m_attachmentTemp1 = renderTargets.MakeAttachment(COLOR_FORMAT, "SSRTemp1");
	m_attachmentTemp2 = renderTargets.MakeAttachment(COLOR_FORMAT, "SSRTemp2");
	m_attachmentDepth = renderTargets.MakeAttachment(DEPTH_FORMAT, "SSRDepth", eg::TextureFlags::TransientAttachment);

	m_framebufferTemp1 = eg::Framebuffer::CreateBasic(m_attachmentTemp1, m_attachmentDepth);
	m_framebufferTemp2 = eg::Framebuffer::CreateBasic(m_attachmentTemp2);

	m_initialPassAttachmentsDS = eg::DescriptorSet(m_pipelineInitial, 1);
	m_initialPassAttachmentsDS.BindTexture(renderTargets.ssrColorInput, 0);
	m_initialPassAttachmentsDS.BindTexture(renderTargets.gbufferColor2, 1);
	m_initialPassAttachmentsDS.BindTexture(renderTargets.gbufferDepth, 2, {}, eg::TextureUsage::DepthStencilReadOnly);
	m_initialPassAttachmentsDS.BindTexture(renderTargets.GetWaterDepthTextureOrDummy(), 3);
	m_initialPassAttachmentsDS.BindSampler(samplers::linearClamp, 4);
	m_initialPassAttachmentsDS.BindSampler(samplers::nearestClamp, 5);

	m_blendPassAttachmentsDS = eg::DescriptorSet(m_pipelineBlendPass, 1);
	m_blendPassAttachmentsDS.BindTexture(renderTargets.gbufferColor1, 0);
	m_blendPassAttachmentsDS.BindTexture(renderTargets.gbufferColor2, 1);
	m_blendPassAttachmentsDS.BindTexture(renderTargets.gbufferDepth, 2, {}, eg::TextureUsage::DepthStencilReadOnly);
	m_blendPassAttachmentsDS.BindSampler(samplers::nearestClamp, 3);

	m_blurPass1AttachmentsDS = eg::DescriptorSet(m_blur1Pipeline, 1);
	m_blurPass1AttachmentsDS.BindSampler(samplers::linearClamp, 0);
	m_blurPass1AttachmentsDS.BindTexture(m_attachmentTemp1, 1);

	m_blurPass2AttachmentsDS = eg::DescriptorSet(m_blur2Pipeline, 1);
	m_blurPass2AttachmentsDS.BindSampler(samplers::linearClamp, 0);
	m_blurPass2AttachmentsDS.BindTexture(m_attachmentTemp2, 1);
	m_blurPass2AttachmentsDS.BindTexture(renderTargets.ssrColorInput, 2);
}

void SSR::Render(const SSRRenderArgs& renderArgs)
{
	const float fallbackColorAndIntensity[4] = {
		renderArgs.fallbackColor.r,
		renderArgs.fallbackColor.g,
		renderArgs.fallbackColor.b,
		renderArgs.intensity,
	};
	const uint32_t frameUniformDataOffset = PushFrameUniformData(ToCharSpan<float>(fallbackColorAndIntensity));

	eg::MultiStageGPUTimer timer;

	// ** Initial rendering **
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = m_framebufferTemp1.handle;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.depthStoreOp = eg::AttachmentStoreOp::Discard;
	rpBeginInfo.depthClearValue = 1;
	eg::DC.BeginRenderPass(rpBeginInfo);

	timer.StartStage("Initial");

	eg::DC.BindPipeline(m_pipelineInitial);
	eg::DC.BindDescriptorSet(m_initialPassBuffersDS, 0, { &frameUniformDataOffset, 1 });
	eg::DC.BindDescriptorSet(m_initialPassAttachmentsDS, 1);
	eg::DC.Draw(0, 3, 0, 1);

	timer.StartStage("Additional");

	if (renderArgs.renderAdditionalCallback)
		renderArgs.renderAdditionalCallback();

	timer.StartStage("Blend");

	// ** Blend pass **
	eg::DC.BindPipeline(m_pipelineBlendPass);
	eg::DC.BindDescriptorSet(m_initialPassBuffersDS, 0, { &frameUniformDataOffset, 1 });
	eg::DC.BindDescriptorSet(m_blendPassAttachmentsDS, 1);
	eg::DC.Draw(0, 3, 0, 1);

	eg::DC.EndRenderPass();

	m_attachmentTemp1.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);

	// ** First blur pass **
	timer.StartStage("Blur 1");
	rpBeginInfo.framebuffer = m_framebufferTemp2.handle;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	eg::DC.BeginRenderPass(rpBeginInfo);
	eg::DC.BindPipeline(m_blur1Pipeline);
	eg::DC.BindDescriptorSet(m_blurParametersBufferDS, 0);
	eg::DC.BindDescriptorSet(m_blurPass1AttachmentsDS, 1);
	eg::DC.Draw(0, 3, 0, 1);
	eg::DC.EndRenderPass();

	m_attachmentTemp2.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);

	// ** Second blur pass **
	timer.StartStage("Blur 2");
	rpBeginInfo.framebuffer = renderArgs.renderTargets->ssrStage->framebuffer.handle;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Load;
	rpBeginInfo.depthStencilReadOnly = true;
	eg::DC.BeginRenderPass(rpBeginInfo);
	eg::DC.BindPipeline(m_blur2Pipeline);
	eg::DC.BindDescriptorSet(m_blurParametersBufferDS, 0);
	eg::DC.BindDescriptorSet(m_blurPass2AttachmentsDS, 1);
	eg::DC.Draw(0, 3, 0, 1);

	// Intentially no end render pass
}
