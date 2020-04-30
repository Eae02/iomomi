#include "DeferredRenderer.hpp"
#include "Lighting/LightMeshes.hpp"
#include "RenderSettings.hpp"
#include "PlanarReflectionsManager.hpp"
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

//Stores a pair of two SSR sample counts (for the linear search and the binary search respectively)
// for each each reflection quality level. 0 means SSR is disabled
std::pair<uint32_t, uint32_t> SSR_SAMPLES[] = 
{
	/* VeryLow  */ { 0, 0 },
	/* Low      */ { 0, 0 },
	/* Medium   */ { 4, 6 },
	/* High     */ { 6, 6 },
	/* VeryHigh */ { 8, 8 }
};

void DeferredRenderer::CreatePipelines()
{
	eg::StencilState ambientStencilState;
	ambientStencilState.failOp = eg::StencilOp::Keep;
	ambientStencilState.passOp = eg::StencilOp::Keep;
	ambientStencilState.depthFailOp = eg::StencilOp::Keep;
	ambientStencilState.compareOp = eg::CompareOp::Equal;
	ambientStencilState.writeMask = 0;
	ambientStencilState.compareMask = 1;
	ambientStencilState.reference = 0;
	
	eg::SpecializationConstantEntry ambientSpecConstEntries[2];
	ambientSpecConstEntries[0].constantID = 0;
	ambientSpecConstEntries[0].size = sizeof(uint32_t);
	ambientSpecConstEntries[0].offset = offsetof(std::decay_t<decltype(SSR_SAMPLES[0])>, first);
	ambientSpecConstEntries[1].constantID = 1;
	ambientSpecConstEntries[1].size = sizeof(uint32_t);
	ambientSpecConstEntries[1].offset = offsetof(std::decay_t<decltype(SSR_SAMPLES[0])>, second);
	
	eg::GraphicsPipelineCreateInfo ambientPipelineCI;
	ambientPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").DefaultVariant();
	ambientPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/Ambient.fs.glsl").DefaultVariant();
	ambientPipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::One);
	ambientPipelineCI.fragmentShader.specConstants = ambientSpecConstEntries;
	ambientPipelineCI.fragmentShader.specConstantsData = &SSR_SAMPLES[(int)settings.reflectionsQuality];
	ambientPipelineCI.fragmentShader.specConstantsDataSize = sizeof(SSR_SAMPLES[0]);
	ambientPipelineCI.frontStencilState = ambientStencilState;
	ambientPipelineCI.backStencilState = ambientStencilState;
	ambientPipelineCI.enableStencilTest = true;
	m_ambientPipeline = eg::Pipeline::Create(ambientPipelineCI);
	m_ambientPipeline.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR);
	m_ambientPipeline.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR);
	
	eg::GraphicsPipelineCreateInfo slPipelineCI;
	slPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/SpotLight.vs.glsl").DefaultVariant();
	slPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/SpotLight.fs.glsl").DefaultVariant();
	slPipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::One);
	slPipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, 0 };
	slPipelineCI.vertexBindings[0] = { sizeof(float) * 3, eg::InputRate::Vertex };
	slPipelineCI.cullMode = eg::CullMode::Front;
	m_spotLightPipeline = eg::Pipeline::Create(slPipelineCI);
	m_spotLightPipeline.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR);
	m_spotLightPipeline.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR);
	
	eg::SpecializationConstantEntry pointLightSpecConstEntries[1];
	pointLightSpecConstEntries[0].constantID = 0;
	pointLightSpecConstEntries[0].size = sizeof(uint32_t);
	pointLightSpecConstEntries[0].offset = 0;
	uint32_t PL_SPEC_CONST_DATA_SOFT_SHADOWS = 1;
	uint32_t PL_SPEC_CONST_DATA_HARD_SHADOWS = 0;
	
	eg::GraphicsPipelineCreateInfo plPipelineCI;
	plPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/PointLight.vs.glsl").DefaultVariant();
	plPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/PointLight.fs.glsl").DefaultVariant();
	plPipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::One);
	plPipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, 0 };
	plPipelineCI.vertexBindings[0] = { sizeof(float) * 3, eg::InputRate::Vertex };
	plPipelineCI.cullMode = eg::CullMode::Back;
	plPipelineCI.fragmentShader.specConstants = pointLightSpecConstEntries;
	plPipelineCI.fragmentShader.specConstantsData = &PL_SPEC_CONST_DATA_SOFT_SHADOWS;
	plPipelineCI.fragmentShader.specConstantsDataSize = sizeof(uint32_t);
	m_pointLightPipelineSoftShadows = eg::Pipeline::Create(plPipelineCI);
	m_pointLightPipelineSoftShadows.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR);
	m_pointLightPipelineSoftShadows.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR);
	
	plPipelineCI.fragmentShader.specConstantsData = &PL_SPEC_CONST_DATA_HARD_SHADOWS;
	m_pointLightPipelineHardShadows = eg::Pipeline::Create(plPipelineCI);
	m_pointLightPipelineHardShadows.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR);
	m_pointLightPipelineHardShadows.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR);
	
	const eg::ShaderModuleAsset& reflPlaneFS = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/ReflectionPlane.fs.glsl");
	
	eg::StencilState reflPlaneStencilState;
	reflPlaneStencilState.failOp = eg::StencilOp::Keep;
	reflPlaneStencilState.passOp = eg::StencilOp::Keep;
	reflPlaneStencilState.depthFailOp = eg::StencilOp::Keep;
	reflPlaneStencilState.compareOp = eg::CompareOp::Equal;
	reflPlaneStencilState.writeMask = 0;
	reflPlaneStencilState.compareMask = 1;
	reflPlaneStencilState.reference = 1;
	
	eg::GraphicsPipelineCreateInfo reflPlanePipelineCI;
	reflPlanePipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/ReflectionPlane.vs.glsl").DefaultVariant();
	reflPlanePipelineCI.fragmentShader = reflPlaneFS.DefaultVariant();
	reflPlanePipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::One);
	reflPlanePipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 2, 0 };
	reflPlanePipelineCI.vertexBindings[0] = { sizeof(float) * 2, eg::InputRate::Vertex };
	reflPlanePipelineCI.cullMode = eg::CullMode::None;
	reflPlanePipelineCI.topology = eg::Topology::TriangleStrip;
	reflPlanePipelineCI.enableStencilTest = true;
	reflPlanePipelineCI.frontStencilState = reflPlaneStencilState;
	reflPlanePipelineCI.backStencilState = reflPlaneStencilState;
	m_reflectionPlanePipeline = eg::Pipeline::Create(reflPlanePipelineCI);
	m_reflectionPlanePipeline.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR);
	m_reflectionPlanePipeline.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR);
	
	constexpr float REFLECTION_PLANE_SIZE = 10000;
	const float reflectionPlaneVertices[] =
	{
		-REFLECTION_PLANE_SIZE, -REFLECTION_PLANE_SIZE,
		 REFLECTION_PLANE_SIZE, -REFLECTION_PLANE_SIZE,
		-REFLECTION_PLANE_SIZE,  REFLECTION_PLANE_SIZE,
		 REFLECTION_PLANE_SIZE,  REFLECTION_PLANE_SIZE,
	};
	m_reflectionPlaneVertexBuffer = eg::Buffer(eg::BufferFlags::VertexBuffer,
		sizeof(reflectionPlaneVertices), reflectionPlaneVertices);
	m_reflectionPlaneVertexBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
}

DeferredRenderer::RenderTarget::RenderTarget(uint32_t width, uint32_t height,
	eg::TextureRef outputTexture, uint32_t outputArrayLayer, QualityLevel waterQuality)
{
	m_width = width;
	m_height = height;
	m_waterOutputTexture = outputTexture;
	
	eg::TextureCreateInfo colorTexCreateInfo;
	colorTexCreateInfo.flags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample;
	colorTexCreateInfo.width = m_width;
	colorTexCreateInfo.height = m_height;
	colorTexCreateInfo.mipLevels = 1;
	colorTexCreateInfo.format = eg::Format::R8G8B8A8_UNorm;
	colorTexCreateInfo.defaultSamplerDescription = &s_attachmentSamplerDesc;
	m_gbColor1Texture = eg::Texture::Create2D(colorTexCreateInfo);
	m_gbColor2Texture = eg::Texture::Create2D(colorTexCreateInfo);
	
	eg::TextureCreateInfo depthTexCreateInfo;
	depthTexCreateInfo.flags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample | eg::TextureFlags::CopySrc;
	depthTexCreateInfo.width = m_width;
	depthTexCreateInfo.height = m_height;
	depthTexCreateInfo.mipLevels = 1;
	depthTexCreateInfo.format = DEPTH_FORMAT;
	depthTexCreateInfo.defaultSamplerDescription = &s_attachmentSamplerDesc;
	m_gbDepthTexture = eg::Texture::Create2D(depthTexCreateInfo);
	
	eg::FramebufferAttachment gbColorAttachments[] = { m_gbColor1Texture.handle, m_gbColor2Texture.handle };
	
	eg::FramebufferCreateInfo gbFramebufferCI;
	gbFramebufferCI.colorAttachments = gbColorAttachments;
	gbFramebufferCI.depthStencilAttachment = m_gbDepthTexture.handle;
	m_gbFramebuffer = eg::Framebuffer(gbFramebufferCI);
	
	
	eg::TextureCreateInfo waterInputTexCreateInfo;
	waterInputTexCreateInfo.flags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample;
	waterInputTexCreateInfo.width = m_width;
	waterInputTexCreateInfo.height = m_height;
	waterInputTexCreateInfo.mipLevels = 1;
	waterInputTexCreateInfo.format = DeferredRenderer::LIGHT_COLOR_FORMAT_HDR;
	waterInputTexCreateInfo.defaultSamplerDescription = &s_attachmentSamplerDesc;
	m_lightingOutputTexture = eg::Texture::Create2D(waterInputTexCreateInfo);
	
	
	eg::FramebufferAttachment lightFBAttachments[1] = { m_lightingOutputTexture.handle };
	m_lightingFramebuffer = eg::Framebuffer(lightFBAttachments, m_gbDepthTexture.handle);
	
	eg::FramebufferAttachment lightFBAttachmentsNoWater[1] = { outputTexture.handle };
	m_lightingFramebufferNoWater = eg::Framebuffer(lightFBAttachmentsNoWater, m_gbDepthTexture.handle);
	
	eg::FramebufferCreateInfo emissiveFramebufferCI;
	eg::FramebufferCreateInfo emissiveFramebufferNoWaterCI;
	emissiveFramebufferCI.depthStencilAttachment = m_gbDepthTexture.handle;
	emissiveFramebufferNoWaterCI.depthStencilAttachment = m_gbDepthTexture.handle;
	emissiveFramebufferCI.colorAttachments = lightFBAttachments;
	emissiveFramebufferNoWaterCI.colorAttachments = lightFBAttachmentsNoWater;
	
	m_emissiveFramebuffer = eg::Framebuffer(emissiveFramebufferCI);
	m_emissiveFramebufferNoWater = eg::Framebuffer(emissiveFramebufferNoWaterCI);
	
	m_waterRT = WaterRenderer::RenderTarget(width, height, m_lightingOutputTexture,
		DepthTexture(), outputTexture, outputArrayLayer, waterQuality);
}

void DeferredRenderer::PollSettingsChanged()
{
	if (settings.reflectionsQuality != m_currentReflectionQualityLevel)
	{
		m_currentReflectionQualityLevel = settings.reflectionsQuality;
		CreatePipelines();
	}
}

void DeferredRenderer::BeginGeometry(RenderTarget& target) const
{
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = target.m_gbFramebuffer.handle;
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.depthClearValue = 1.0f;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.colorAttachments[1].loadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.colorAttachments[0].clearValue = eg::ColorSRGB(0.3f, 0.1f, 0.1f, 1);
	rpBeginInfo.colorAttachments[1].clearValue = eg::ColorLin(0, 0, 1, 1);
	eg::DC.BeginRenderPass(rpBeginInfo);
}

void DeferredRenderer::BeginEmissive(DeferredRenderer::RenderTarget& target, bool hasWater)
{
	eg::DC.EndRenderPass();
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = hasWater ? target.m_emissiveFramebuffer.handle : target.m_emissiveFramebufferNoWater.handle;
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Load;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.colorAttachments[0].clearValue = eg::ColorLin(eg::Color::Black);
	
	eg::DC.BeginRenderPass(rpBeginInfo);
}

void DeferredRenderer::BeginLighting(RenderTarget& target, bool hasWater) const
{
	eg::DC.EndRenderPass();
	
	target.m_gbColor1Texture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	target.m_gbColor2Texture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	target.m_gbDepthTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = hasWater ? target.m_lightingFramebuffer.handle : target.m_lightingFramebufferNoWater.handle;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Load;
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Load;
	rpBeginInfo.sampledDepthStencil = true;
	
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	eg::ColorLin ambientColor = eg::ColorLin(eg::ColorSRGB::FromHex(0xf6f9fc));
	eg::DC.BindPipeline(m_ambientPipeline);
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	eg::DC.BindTexture(target.m_gbColor1Texture, 0, 1);
	eg::DC.BindTexture(target.m_gbColor2Texture, 0, 2);
	eg::DC.BindTexture(target.m_gbDepthTexture, 0, 3);
	ambientColor = ambientColor.ScaleRGB(0.2f);
	
	eg::DC.PushConstants(0, sizeof(float) * 3, &ambientColor.r);
	
	eg::DC.Draw(0, 3, 0, 1);
}

void DeferredRenderer::End(RenderTarget& target) const
{
	eg::DC.EndRenderPass();
}

void DeferredRenderer::DrawWaterBasic(eg::BufferRef positionsBuffer, uint32_t numParticles) const
{
	if (numParticles > 0)
	{
		m_waterRenderer.RenderBasic(positionsBuffer, numParticles);
	}
}

void DeferredRenderer::DrawWater(RenderTarget& target, eg::BufferRef positionsBuffer, uint32_t numParticles)
{
	target.m_lightingOutputTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	
	m_waterRenderer.Render(positionsBuffer, numParticles, target.m_waterRT);
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

void DeferredRenderer::DrawReflectionPlaneLighting(RenderTarget& target, const std::vector<ReflectionPlane*>& planes)
{
	if (planes.empty())
		return;
	
	eg::DC.BindPipeline(m_reflectionPlanePipeline);
	
	eg::DC.BindVertexBuffer(0, m_reflectionPlaneVertexBuffer, 0);
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	
	eg::DC.BindTexture(target.m_gbColor1Texture, 0, 1);
	eg::DC.BindTexture(target.m_gbColor2Texture, 0, 2);
	eg::DC.BindTexture(target.m_gbDepthTexture, 0, 3);
	eg::DC.BindTexture(m_brdfIntegMap.GetTexture(), 0, 4);
	
	for (const ReflectionPlane* plane : planes)
	{
		eg::DC.BindTexture(plane->texture, 0, 5);
		
		float pc[4];
		pc[0] = plane->plane.GetNormal().x;
		pc[1] = plane->plane.GetNormal().y;
		pc[2] = plane->plane.GetNormal().z;
		pc[3] = plane->plane.GetDistance();
		
		eg::DC.PushConstants(0, sizeof(pc), pc);
		
		eg::DC.Draw(0, 4, 0, 1);
	}
}
