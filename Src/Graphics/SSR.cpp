#include "SSR.hpp"
#include "GraphicsCommon.hpp"
#include "RenderSettings.hpp"
#include "../Settings.hpp"

void SSR::CreatePipeline()
{
	const eg::ShaderModuleAsset& ssrBlurShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/SSRBlur.fs.glsl");
	eg::ShaderModuleHandle postVertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").DefaultVariant();
	
	eg::SpecializationConstantEntry specConstEntries[3];
	specConstEntries[0].constantID = 0;
	specConstEntries[0].size = sizeof(uint32_t);
	specConstEntries[0].offset = 0 * sizeof(uint32_t);
	specConstEntries[1].constantID = 1;
	specConstEntries[1].size = sizeof(uint32_t);
	specConstEntries[1].offset = 1 * sizeof(uint32_t);
	specConstEntries[2].constantID = 2;
	specConstEntries[2].size = sizeof(uint32_t);
	specConstEntries[2].offset = 2 * sizeof(uint32_t);
	
	uint32_t blurRadius = qvar::ssrBlurRadius(settings.reflectionsQuality);
	uint32_t specConstantData[3] =
	{
		qvar::ssrLinearSamples(settings.reflectionsQuality),
		qvar::ssrBinSearchSamples(settings.reflectionsQuality),
		blurRadius == 0
	};
	
	eg::GraphicsPipelineCreateInfo pipeline1CI;
	pipeline1CI.vertexShader = postVertexShader;
	pipeline1CI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/SSR.fs.glsl").DefaultVariant();
	pipeline1CI.fragmentShader.specConstants = specConstEntries;
	pipeline1CI.fragmentShader.specConstantsData = specConstantData;
	pipeline1CI.fragmentShader.specConstantsDataSize = sizeof(specConstantData);
	m_pipelineInitial = eg::Pipeline::Create(pipeline1CI);
	m_pipelineInitial.FramebufferFormatHint(GetFormatForRenderTexture(RenderTex::SSRTemp1));
	
	eg::SpecializationConstantEntry specConstEntryBlur;
	specConstEntryBlur.constantID = 0;
	specConstEntryBlur.size = sizeof(uint32_t);
	specConstEntryBlur.offset = 0;
	
	eg::GraphicsPipelineCreateInfo pipelineBlurCI;
	pipelineBlurCI.vertexShader = postVertexShader;
	pipelineBlurCI.fragmentShader = ssrBlurShader.GetVariant("VPass1");
	pipelineBlurCI.fragmentShader.specConstants = { &specConstEntryBlur, 1 };
	pipelineBlurCI.fragmentShader.specConstantsData = &blurRadius;
	pipelineBlurCI.fragmentShader.specConstantsDataSize = sizeof(uint32_t);
	m_blur1Pipeline = eg::Pipeline::Create(pipelineBlurCI);
	m_blur1Pipeline.FramebufferFormatHint(GetFormatForRenderTexture(RenderTex::SSRTemp2));
	
	pipelineBlurCI.fragmentShader = ssrBlurShader.GetVariant("VPass2");
	m_blur2Pipeline = eg::Pipeline::Create(pipelineBlurCI);
	m_blur2Pipeline.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR);
	m_blur2Pipeline.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR);
}

float* ssrBlurIntensity = eg::TweakVarFloat("ssr_blur_scale", 0.05f, 0.0f);
float* ssrBlurFalloff = eg::TweakVarFloat("ssr_blur_fo", 5.0f, 0.0f);
float* ssrBlurMax = eg::TweakVarFloat("ssr_blur_max", 4.0f, 0.0f);

void SSR::Render(eg::TextureRef waterDepth, RenderTex destinationTexture, RenderTexManager& rtManager,
	const eg::ColorSRGB& fallbackColor, float intensityScale)
{
	if (settings.reflectionsQuality != m_currentReflectionQualityLevel)
	{
		CreatePipeline();
		m_currentReflectionQualityLevel = settings.reflectionsQuality;
	}
	
	const bool renderDirect = qvar::ssrBlurRadius(settings.reflectionsQuality) == 0;
	RenderTex initialOutTexture = renderDirect ? destinationTexture : RenderTex::SSRTemp1;
	
	// ** Initial rendering **
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = rtManager.GetFramebuffer(initialOutTexture, {}, {}, "SSR");
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	eg::ColorLin fallbackColorLin(fallbackColor);
	const float pcInitial[4] = { fallbackColorLin.r, fallbackColorLin.g, fallbackColorLin.b, intensityScale };
	
	eg::DC.BindPipeline(m_pipelineInitial);
	eg::DC.PushConstants(0, sizeof(pcInitial), pcInitial);
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::LitWithoutSSR), 0, 1, &framebufferLinearSampler);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::GBColor1), 0, 2);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::GBColor2), 0, 3);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::GBDepth), 0, 4);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::Flags), 0, 5);
	eg::DC.BindTexture(waterDepth, 0, 6);
	
	eg::DC.Draw(0, 3, 0, 1);
	
	eg::DC.EndRenderPass();
	rtManager.RenderTextureUsageHintFS(initialOutTexture);
	if (renderDirect)
		return;
	
	// ** First blur pass **
	rpBeginInfo.framebuffer = rtManager.GetFramebuffer(RenderTex::SSRTemp2, {}, {}, "SSR-B1");
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	eg::DC.BeginRenderPass(rpBeginInfo);
	eg::DC.BindPipeline(m_blur1Pipeline);
	
	const float relativeRadius =
		(float)qvar::ssrBlurRadius(QualityLevel::VeryHigh) /
		(float)qvar::ssrBlurRadius(settings.reflectionsQuality);
	const float maxBlur = *ssrBlurMax * relativeRadius;
	const float falloff = *ssrBlurFalloff;
	const float pc1[4] = { *ssrBlurIntensity, 0, falloff, maxBlur };
	eg::DC.PushConstants(0, sizeof(pc1), pc1);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::SSRTemp1), 0, 0, &framebufferLinearSampler);
	eg::DC.Draw(0, 3, 0, 1);
	eg::DC.EndRenderPass();
	rtManager.RenderTextureUsageHintFS(RenderTex::SSRTemp2);
	
	// ** Second blur pass **
	rpBeginInfo.framebuffer = rtManager.GetFramebuffer(destinationTexture, {}, {}, "SSR-B2");
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	eg::DC.BeginRenderPass(rpBeginInfo);
	eg::DC.BindPipeline(m_blur2Pipeline);
	
	const float pc2[4] = { 0, *ssrBlurIntensity * (float)rtManager.ResY() / (float)rtManager.ResX(), falloff, maxBlur };
	eg::DC.PushConstants(0, sizeof(pc2), pc2);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::SSRTemp2), 0, 0, &framebufferLinearSampler);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::LitWithoutSSR), 0, 1);
	eg::DC.Draw(0, 3, 0, 1);
	eg::DC.EndRenderPass();
	rtManager.RenderTextureUsageHintFS(destinationTexture);
}
