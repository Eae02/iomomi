#include "SSR.hpp"

#include "../Settings.hpp"
#include "GraphicsCommon.hpp"
#include "RenderSettings.hpp"

const float SSR::MAX_DISTANCE = 20;

void SSR::CreatePipeline()
{
	const eg::ShaderModuleAsset& ssrBlurShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/SSRBlur.fs.glsl");
	eg::ShaderStageInfo postVertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").ToStageInfo();

	uint32_t linearSamples = qvar::ssrLinearSamples(settings.reflectionsQuality);
	uint32_t binarySamples = qvar::ssrBinSearchSamples(settings.reflectionsQuality);

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
	pipeline1CI.colorAttachmentFormats[0] = GetFormatForRenderTexture(RenderTex::SSRTemp1, true);
	pipeline1CI.depthAttachmentFormat = GetFormatForRenderTexture(RenderTex::SSRDepth, true);
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
	pipelineBlendPassCI.colorAttachmentFormats[0] = GetFormatForRenderTexture(RenderTex::SSRTemp1, true);
	pipelineBlendPassCI.depthAttachmentFormat = GetFormatForRenderTexture(RenderTex::SSRDepth, true);
	pipelineBlendPassCI.label = "SSR[Blend]";
	m_pipelineBlendPass = eg::Pipeline::Create(pipelineBlendPassCI);

	const uint32_t blurRadius = qvar::ssrBlurRadius(settings.reflectionsQuality);
	const eg::SpecializationConstantEntry specConstantsBlur[] = { { 0, blurRadius } };

	eg::GraphicsPipelineCreateInfo pipelineBlurCI;
	pipelineBlurCI.vertexShader = postVertexShader;
	pipelineBlurCI.fragmentShader = {
		.shaderModule = ssrBlurShader.GetVariant("VPass1"),
		.specConstants = specConstantsBlur,
	};
	pipelineBlurCI.colorAttachmentFormats[0] = GetFormatForRenderTexture(RenderTex::SSRTemp2, true);
	pipelineBlurCI.label = "SSR[Blur1]";
	m_blur1Pipeline = eg::Pipeline::Create(pipelineBlurCI);

	pipelineBlurCI.fragmentShader.shaderModule = ssrBlurShader.GetVariant("VPass2");
	pipelineBlurCI.colorAttachmentFormats[0] = lightColorAttachmentFormat;
	pipelineBlurCI.label = "SSR[Blur2]";
	m_blur2Pipeline = eg::Pipeline::Create(pipelineBlurCI);
}

static float* ssrBlurIntensity = eg::TweakVarFloat("ssr_blur_scale", 0.05f, 0.0f);
static float* ssrBlurFalloff = eg::TweakVarFloat("ssr_blur_fo", 5.0f, 0.0f);
static float* ssrBlurMax = eg::TweakVarFloat("ssr_blur_max", 4.0f, 0.0f);

struct SSRPushConstants
{
	float blurDirX;
	float blurDirY;
	float maxBlurDist;
	float _padding;
	float blurCoeff[16];
};

void SSR::Render(const SSRRenderArgs& renderArgs)
{
	if (settings.reflectionsQuality != m_currentReflectionQualityLevel)
	{
		CreatePipeline();
		m_currentReflectionQualityLevel = settings.reflectionsQuality;
	}

	const uint32_t blurRadius = qvar::ssrBlurRadius(settings.reflectionsQuality);

	SSRPushConstants pc = {};
	float coeffSum = 1;
	pc.blurCoeff[0] = 1;
	for (uint32_t i = 1; i < blurRadius; i++)
	{
		float r = (static_cast<float>(i) * *ssrBlurFalloff) / static_cast<float>(blurRadius - 1);
		pc.blurCoeff[i] = std::exp(-r * r);
		coeffSum += 2 * pc.blurCoeff[i];
	}
	for (uint32_t i = 0; i < blurRadius; i++)
	{
		pc.blurCoeff[i] /= coeffSum;
	}

	eg::MultiStageGPUTimer timer;

	// ** Initial rendering **
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = renderArgs.rtManager->GetFramebuffer(RenderTex::SSRTemp1, {}, RenderTex::SSRDepth, "SSR");
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.depthClearValue = 1;
	eg::DC.BeginRenderPass(rpBeginInfo);

	timer.StartStage("Initial");

	eg::DC.BindPipeline(m_pipelineInitial);
	eg::DC.PushConstants(0, sizeof(float) * 3, &renderArgs.fallbackColor.r);
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0);
	eg::DC.BindTexture(
		renderArgs.rtManager->GetRenderTexture(RenderTex::LitWithoutSSR), 0, 1, &framebufferLinearSampler
	);
	eg::DC.BindTexture(renderArgs.rtManager->GetRenderTexture(RenderTex::GBColor2), 0, 2, &framebufferNearestSampler);
	eg::DC.BindTexture(renderArgs.rtManager->GetRenderTexture(RenderTex::GBDepth), 0, 3, &framebufferNearestSampler);
	eg::DC.BindTexture(renderArgs.waterDepth, 0, 4, &framebufferNearestSampler);

	eg::DC.Draw(0, 3, 0, 1);

	timer.StartStage("Additional");

	if (renderArgs.renderAdditionalCallback && settings.reflectionsQuality >= QualityLevel::High)
	{
		renderArgs.renderAdditionalCallback();
	}

	timer.StartStage("Blend");

	// ** Blend pass **
	eg::DC.BindPipeline(m_pipelineBlendPass);
	eg::DC.PushConstants(0, sizeof(float), &renderArgs.intensity);
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0);
	eg::DC.BindTexture(renderArgs.rtManager->GetRenderTexture(RenderTex::GBColor1), 0, 1, &framebufferNearestSampler);
	eg::DC.BindTexture(renderArgs.rtManager->GetRenderTexture(RenderTex::GBColor2), 0, 2, &framebufferNearestSampler);
	eg::DC.BindTexture(renderArgs.rtManager->GetRenderTexture(RenderTex::GBDepth), 0, 3, &framebufferNearestSampler);
	eg::DC.Draw(0, 3, 0, 1);

	eg::DC.EndRenderPass();
	renderArgs.rtManager->RenderTextureUsageHintFS(RenderTex::SSRTemp1);

	// ** First blur pass **
	rpBeginInfo.framebuffer = renderArgs.rtManager->GetFramebuffer(RenderTex::SSRTemp2, {}, {}, "SSR-B1");
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	eg::DC.BeginRenderPass(rpBeginInfo);
	eg::DC.BindPipeline(m_blur1Pipeline);

	timer.StartStage("Blur 1");

	const float relativeRadius = (float)qvar::ssrBlurRadius(QualityLevel::VeryHigh) / static_cast<float>(blurRadius);
	const float blurDistance =
		*ssrBlurIntensity * MAX_DISTANCE / (float)qvar::ssrBlurRadius(settings.reflectionsQuality);
	const float maxBlur = *ssrBlurMax * relativeRadius;
	pc.blurDirX = blurDistance;
	pc.blurDirY = 0;
	pc.maxBlurDist = maxBlur;
	eg::DC.PushConstants(0, sizeof(pc), &pc);
	eg::DC.BindTexture(renderArgs.rtManager->GetRenderTexture(RenderTex::SSRTemp1), 0, 0, &framebufferLinearSampler);
	eg::DC.Draw(0, 3, 0, 1);
	eg::DC.EndRenderPass();
	renderArgs.rtManager->RenderTextureUsageHintFS(RenderTex::SSRTemp2);

	// ** Second blur pass **
	rpBeginInfo.framebuffer = renderArgs.rtManager->GetFramebuffer(renderArgs.destinationTexture, {}, {}, "SSR-B2");
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	eg::DC.BeginRenderPass(rpBeginInfo);
	eg::DC.BindPipeline(m_blur2Pipeline);

	timer.StartStage("Blur 2");

	pc.blurDirX = 0;
	pc.blurDirY = blurDistance * (float)renderArgs.rtManager->ResY() / (float)renderArgs.rtManager->ResX();
	eg::DC.PushConstants(0, sizeof(pc), &pc);
	eg::DC.BindTexture(renderArgs.rtManager->GetRenderTexture(RenderTex::SSRTemp2), 0, 0, &framebufferLinearSampler);
	eg::DC.BindTexture(
		renderArgs.rtManager->GetRenderTexture(RenderTex::LitWithoutSSR), 0, 1, &framebufferNearestSampler
	);
	eg::DC.Draw(0, 3, 0, 1);
	eg::DC.EndRenderPass();
	renderArgs.rtManager->RenderTextureUsageHintFS(renderArgs.destinationTexture);
}
