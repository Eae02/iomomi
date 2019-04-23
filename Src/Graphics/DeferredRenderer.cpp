#include "DeferredRenderer.hpp"
#include "Lighting/LightProbesManager.hpp"
#include "Lighting/LightMeshes.hpp"
#include "RenderSettings.hpp"
#include "../Settings.hpp"

const eg::FramebufferFormatHint DeferredRenderer::GEOMETRY_FB_FORMAT =
{
	1,
	DeferredRenderer::DEPTH_FORMAT,
	{ eg::Format::R8G8B8A8_UNorm, eg::Format::R8G8B8A8_UNorm }
};

static eg::SamplerDescription s_attachmentSamplerDesc;

static void OnInit()
{
	s_attachmentSamplerDesc.wrapU = eg::WrapMode::ClampToEdge;
	s_attachmentSamplerDesc.wrapV = eg::WrapMode::ClampToEdge;
	s_attachmentSamplerDesc.wrapW = eg::WrapMode::ClampToEdge;
	s_attachmentSamplerDesc.minFilter = eg::TextureFilter::Nearest;
	s_attachmentSamplerDesc.magFilter = eg::TextureFilter::Nearest;
	s_attachmentSamplerDesc.mipFilter = eg::TextureFilter::Nearest;
}

EG_ON_INIT(OnInit)

DeferredRenderer::DeferredRenderer()
{
	eg::SamplerDescription shadowMapSamplerDesc;
	shadowMapSamplerDesc.wrapU = eg::WrapMode::Repeat;
	shadowMapSamplerDesc.wrapV = eg::WrapMode::Repeat;
	shadowMapSamplerDesc.wrapW = eg::WrapMode::Repeat;
	shadowMapSamplerDesc.minFilter = eg::TextureFilter::Linear;
	shadowMapSamplerDesc.magFilter = eg::TextureFilter::Linear;
	shadowMapSamplerDesc.mipFilter = eg::TextureFilter::Nearest;
	shadowMapSamplerDesc.enableCompare = true;
	shadowMapSamplerDesc.compareOp = eg::CompareOp::Less;
	m_shadowMapSampler = eg::Sampler(shadowMapSamplerDesc);
}

void DeferredRenderer::CreatePipelines()
{
	eg::SpecializationConstantEntry specConstantEntries[1];
	specConstantEntries[0].constantID = 100;
	specConstantEntries[0].size = sizeof(uint32_t);
	specConstantEntries[0].offset = 0;
	
	bool msVariant = eg::CurrentGraphicsAPI() != eg::GraphicsAPI::OpenGL || m_currentSampleCount != 1;
	
	std::string_view variantName = msVariant ? "VMSAA" : "VDefault";
	
	eg::GraphicsPipelineCreateInfo constAmbientPipelineCI;
	constAmbientPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").DefaultVariant();
	constAmbientPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/ConstantAmbient.fs.glsl").GetVariant(variantName);
	constAmbientPipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::One);
	constAmbientPipelineCI.fragmentShader.specConstants = specConstantEntries;
	constAmbientPipelineCI.fragmentShader.specConstantsData = &m_currentSampleCount;
	constAmbientPipelineCI.fragmentShader.specConstantsDataSize = sizeof(uint32_t);
	m_constantAmbientPipeline = eg::Pipeline::Create(constAmbientPipelineCI);
	m_constantAmbientPipeline.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR);
	m_constantAmbientPipeline.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR);
	
	eg::GraphicsPipelineCreateInfo slPipelineCI;
	slPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/SpotLight.vs.glsl").DefaultVariant();
	slPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/SpotLight.fs.glsl").GetVariant(variantName);
	slPipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::One);
	slPipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, 0 };
	slPipelineCI.vertexBindings[0] = { sizeof(float) * 3, eg::InputRate::Vertex };
	slPipelineCI.cullMode = eg::CullMode::Front;
	slPipelineCI.fragmentShader.specConstants = specConstantEntries;
	slPipelineCI.fragmentShader.specConstantsData = &m_currentSampleCount;
	slPipelineCI.fragmentShader.specConstantsDataSize = sizeof(uint32_t);
	m_spotLightPipeline = eg::Pipeline::Create(slPipelineCI);
	m_spotLightPipeline.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR);
	m_spotLightPipeline.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR);
	
	const eg::ShaderModuleAsset& pointLightFS = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/PointLight.fs.glsl");
	
	eg::GraphicsPipelineCreateInfo plPipelineCI;
	plPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/PointLight.vs.glsl").DefaultVariant();
	plPipelineCI.fragmentShader = pointLightFS.GetVariant(msVariant ? "VSoftShadowsMS" : "VSoftShadows");
	plPipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::One);
	plPipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, 0 };
	plPipelineCI.vertexBindings[0] = { sizeof(float) * 3, eg::InputRate::Vertex };
	plPipelineCI.cullMode = eg::CullMode::Back;
	plPipelineCI.fragmentShader.specConstants = specConstantEntries;
	plPipelineCI.fragmentShader.specConstantsData = &m_currentSampleCount;
	plPipelineCI.fragmentShader.specConstantsDataSize = sizeof(uint32_t);
	m_pointLightPipelineSoftShadows = eg::Pipeline::Create(plPipelineCI);
	m_pointLightPipelineSoftShadows.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR);
	m_pointLightPipelineSoftShadows.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR);
	
	plPipelineCI.fragmentShader = pointLightFS.GetVariant(msVariant ? "VHardShadowsMS" : "VHardShadows");
	m_pointLightPipelineHardShadows = eg::Pipeline::Create(plPipelineCI);
	m_pointLightPipelineHardShadows.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR);
	m_pointLightPipelineHardShadows.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR);
}

DeferredRenderer::RenderTarget::RenderTarget(uint32_t width, uint32_t height, uint32_t samples,
	eg::TextureRef outputTexture, uint32_t outputArrayLayer)
{
	m_width = width;
	m_height = height;
	m_outputTexture = outputTexture;
	m_samples = samples;
	
	eg::Texture2DCreateInfo colorTexCreateInfo;
	colorTexCreateInfo.flags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample;
	colorTexCreateInfo.width = m_width;
	colorTexCreateInfo.height = m_height;
	colorTexCreateInfo.sampleCount = samples;
	colorTexCreateInfo.mipLevels = 1;
	colorTexCreateInfo.format = eg::Format::R8G8B8A8_UNorm;
	colorTexCreateInfo.defaultSamplerDescription = &s_attachmentSamplerDesc;
	m_gbColor1Texture = eg::Texture::Create2D(colorTexCreateInfo);
	m_gbColor2Texture = eg::Texture::Create2D(colorTexCreateInfo);
	
	eg::Texture2DCreateInfo depthTexCreateInfo;
	depthTexCreateInfo.flags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample | eg::TextureFlags::CopySrc;
	depthTexCreateInfo.width = m_width;
	depthTexCreateInfo.height = m_height;
	depthTexCreateInfo.sampleCount = samples;
	depthTexCreateInfo.mipLevels = 1;
	depthTexCreateInfo.format = DEPTH_FORMAT;
	depthTexCreateInfo.defaultSamplerDescription = &s_attachmentSamplerDesc;
	m_gbDepthTexture = eg::Texture::Create2D(depthTexCreateInfo);
	
	eg::FramebufferAttachment gbColorAttachments[] = { m_gbColor1Texture.handle, m_gbColor2Texture.handle };
	
	eg::FramebufferCreateInfo gbFramebufferCI;
	gbFramebufferCI.colorAttachments = gbColorAttachments;
	gbFramebufferCI.depthStencilAttachment = m_gbDepthTexture.handle;
	
	m_gbFramebuffer = eg::Framebuffer(gbFramebufferCI);
	
	eg::FramebufferAttachment lightFBAttachments[1];
	lightFBAttachments[0].texture = outputTexture.handle;
	lightFBAttachments[0].subresource.firstArrayLayer = outputArrayLayer;
	lightFBAttachments[0].subresource.numArrayLayers = 1;
	m_lightingFramebuffer = eg::Framebuffer(lightFBAttachments);
	
	eg::FramebufferCreateInfo emissiveFramebufferCI;
	emissiveFramebufferCI.depthStencilAttachment = m_gbDepthTexture.handle;
	
	eg::FramebufferAttachment emissiveColorAttachments[1];
	if (samples > 1)
	{
		eg::Texture2DCreateInfo emissiveTexCreateInfo;
		emissiveTexCreateInfo.flags = eg::TextureFlags::FramebufferAttachment;
		emissiveTexCreateInfo.width = m_width;
		emissiveTexCreateInfo.height = m_height;
		emissiveTexCreateInfo.sampleCount = samples;
		emissiveTexCreateInfo.mipLevels = 1;
		emissiveTexCreateInfo.format = DeferredRenderer::LIGHT_COLOR_FORMAT_HDR;
		m_emissiveTexture = eg::Texture::Create2D(emissiveTexCreateInfo);
		
		emissiveColorAttachments[0] = m_emissiveTexture.handle;
		
		emissiveFramebufferCI.colorAttachments = emissiveColorAttachments;
		emissiveFramebufferCI.colorResolveAttachments = lightFBAttachments;
	}
	else
	{
		emissiveFramebufferCI.colorAttachments = lightFBAttachments;
	}
	
	m_emissiveFramebuffer = eg::Framebuffer(emissiveFramebufferCI);
}

void DeferredRenderer::PollSettingsChanged()
{
	if (settings.msaaSamples != m_currentSampleCount)
	{
		m_currentSampleCount = settings.msaaSamples;
		CreatePipelines();
	}
}

void DeferredRenderer::BeginGeometry(RenderTarget& target) const
{
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = target.m_gbFramebuffer.handle;
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.depthClearValue = 1.0f;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	rpBeginInfo.colorAttachments[1].loadOp = eg::AttachmentLoadOp::Discard;
	eg::DC.BeginRenderPass(rpBeginInfo);
}

void DeferredRenderer::BeginEmissive(DeferredRenderer::RenderTarget& target)
{
	eg::DC.EndRenderPass();
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = target.m_emissiveFramebuffer.handle;
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Load;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.colorAttachments[0].clearValue = eg::ColorLin(eg::Color::Black);
	
	eg::DC.BeginRenderPass(rpBeginInfo);
}

void DeferredRenderer::BeginLighting(RenderTarget& target, const LightProbesManager* lightProbesManager) const
{
	eg::DC.EndRenderPass();
	
	target.m_gbColor1Texture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	target.m_gbColor2Texture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	target.m_gbDepthTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = target.m_lightingFramebuffer.handle;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Load;
	
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	eg::ColorLin ambientColor = eg::ColorLin(eg::ColorSRGB::FromHex(0xf6f9fc));
	if (lightProbesManager == nullptr)
	{
		eg::DC.BindPipeline(m_constantAmbientPipeline);
		eg::DC.BindTexture(target.m_gbColor1Texture, 0, 0);
	}
	else
	{
		lightProbesManager->Bind();
		
		eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
		eg::DC.BindTexture(target.m_gbColor1Texture, 0, 1);
		eg::DC.BindTexture(target.m_gbColor2Texture, 0, 2);
		eg::DC.BindTexture(target.m_gbDepthTexture, 0, 3);
	}
	ambientColor = ambientColor.ScaleRGB(0.1f);
	
	eg::DC.PushConstants(0, sizeof(float) * 3, &ambientColor.r);
	
	eg::DC.Draw(0, 3, 0, 1);
}

void DeferredRenderer::End(RenderTarget& target) const
{
	eg::DC.EndRenderPass();
}

void DeferredRenderer::DrawSpotLights(RenderTarget& target, const std::vector<SpotLightDrawData>& spotLights) const
{
	if (spotLights.empty())
		return;
	
	eg::DC.BindPipeline(m_spotLightPipeline);
	
	BindSpotLightMesh();
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	
	eg::DC.BindTexture(target.m_gbColor1Texture, 0, 1);
	eg::DC.BindTexture(target.m_gbColor2Texture, 0, 2);
	eg::DC.BindTexture(target.m_gbDepthTexture, 0, 3);
	
	for (const SpotLightDrawData& spotLight : spotLights)
	{
		eg::DC.PushConstants(0, spotLight);
		
		eg::DC.DrawIndexed(0, SPOT_LIGHT_MESH_INDICES, 0, 0, 1);
	}
}

void DeferredRenderer::DrawPointLights(RenderTarget& target, const std::vector<PointLightDrawData>& pointLights) const
{
	if (pointLights.empty())
		return;
	
	bool softShadows = settings.shadowQuality >= QualityLevel::Medium;
	
	eg::DC.BindPipeline(softShadows ? m_pointLightPipelineSoftShadows : m_pointLightPipelineHardShadows);
	
	BindPointLightMesh();
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	
	eg::DC.BindTexture(target.m_gbColor1Texture, 0, 1);
	eg::DC.BindTexture(target.m_gbColor2Texture, 0, 2);
	eg::DC.BindTexture(target.m_gbDepthTexture, 0, 3);
	
	for (const PointLightDrawData& pointLight : pointLights)
	{
		eg::DC.BindTexture(pointLight.shadowMap, 0, 4, &m_shadowMapSampler);
		
		eg::DC.PushConstants(0, pointLight.pc);
		
		eg::DC.DrawIndexed(0, POINT_LIGHT_MESH_INDICES, 0, 0, 1);
	}
}
