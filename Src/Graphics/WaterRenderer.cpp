#include "WaterRenderer.hpp"
#include "RenderSettings.hpp"
#include "DeferredRenderer.hpp"
#include "../Settings.hpp"

constexpr uint32_t UNDERWATER_DOWNLOAD_BUFFER_SIZE = sizeof(uint32_t) * eg::MAX_CONCURRENT_FRAMES;

WaterRenderer::WaterRenderer()
{
	float quadVBData[] = { -1, -1, -1, 1, 1, -1, 1, 1 };
	m_quadVB = eg::Buffer(eg::BufferFlags::VertexBuffer, sizeof(quadVBData), quadVBData);
	m_quadVB.UsageHint(eg::BufferUsage::VertexBuffer);
	
	eg::GraphicsPipelineCreateInfo pipelineBasicCI;
	pipelineBasicCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/WaterSphere.vs.glsl").DefaultVariant();
	pipelineBasicCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/WaterSphereBasic.fs.glsl").DefaultVariant();
	pipelineBasicCI.enableDepthTest = true;
	pipelineBasicCI.enableDepthWrite = true;
	pipelineBasicCI.topology = eg::Topology::TriangleStrip;
	pipelineBasicCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 2, 0 };
	pipelineBasicCI.vertexAttributes[1] = { 1, eg::DataType::Float32, 3, 0 };
	pipelineBasicCI.vertexBindings[0] = { sizeof(float) * 2, eg::InputRate::Vertex };
	pipelineBasicCI.vertexBindings[1] = { sizeof(float) * 4, eg::InputRate::Instance };
	m_pipelineBasic = eg::Pipeline::Create(pipelineBasicCI);
	
	eg::GraphicsPipelineCreateInfo pipelineDepthOnlyCI;
	pipelineDepthOnlyCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/WaterSphere.vs.glsl").DefaultVariant();
	pipelineDepthOnlyCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/WaterSphereDepth.fs.glsl").DefaultVariant();
	pipelineDepthOnlyCI.enableDepthTest = true;
	pipelineDepthOnlyCI.enableDepthWrite = true;
	pipelineDepthOnlyCI.topology = eg::Topology::TriangleStrip;
	pipelineDepthOnlyCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 2, 0 };
	pipelineDepthOnlyCI.vertexAttributes[1] = { 1, eg::DataType::Float32, 3, 0 };
	pipelineDepthOnlyCI.vertexBindings[0] = { sizeof(float) * 2, eg::InputRate::Vertex };
	pipelineDepthOnlyCI.vertexBindings[1] = { sizeof(float) * 4, eg::InputRate::Instance };
	m_pipelineDepthOnly = eg::Pipeline::Create(pipelineDepthOnlyCI);
	
	eg::GraphicsPipelineCreateInfo pipelineAddCI;
	pipelineAddCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/WaterSphere.vs.glsl").DefaultVariant();
	pipelineAddCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/WaterSphereAdd.fs.glsl").DefaultVariant();
	pipelineAddCI.topology = eg::Topology::TriangleStrip;
	pipelineAddCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 2, 0 };
	pipelineAddCI.vertexAttributes[1] = { 1, eg::DataType::Float32, 3, 0 };
	pipelineAddCI.vertexBindings[0] = { sizeof(float) * 2, eg::InputRate::Vertex };
	pipelineAddCI.vertexBindings[1] = { sizeof(float) * 4, eg::InputRate::Instance };
	pipelineAddCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::One);
	m_pipelineAdditive = eg::Pipeline::Create(pipelineAddCI);
	
	auto& postFS = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/WaterPost.fs.glsl");
	eg::GraphicsPipelineCreateInfo pipelinePostCI;
	pipelinePostCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").DefaultVariant();
	pipelinePostCI.fragmentShader = postFS.GetVariant("VLowQual");
	m_pipelinePostLowQual = eg::Pipeline::Create(pipelinePostCI);
	
	pipelinePostCI.fragmentShader = postFS.GetVariant("VStdQual");
	m_pipelinePostStdQual = eg::Pipeline::Create(pipelinePostCI);
	
	pipelinePostCI.fragmentShader = postFS.GetVariant("VHighQual");
	m_pipelinePostHighQual = eg::Pipeline::Create(pipelinePostCI);
	
	m_pipelinesRefPost[(int)QualityLevel::VeryLow]  = m_pipelinePostLowQual;
	m_pipelinesRefPost[(int)QualityLevel::Low]      = m_pipelinePostStdQual;
	m_pipelinesRefPost[(int)QualityLevel::Medium]   = m_pipelinePostStdQual;
	m_pipelinesRefPost[(int)QualityLevel::High]     = m_pipelinePostHighQual;
	m_pipelinesRefPost[(int)QualityLevel::VeryHigh] = m_pipelinePostHighQual;
	
	m_currentQualityLevel = (QualityLevel)-1;
	
	m_normalMapTexture = &eg::GetAsset<eg::Texture>("Textures/WaterN.png");
}

void WaterRenderer::CreateDepthBlurPipelines(uint32_t samples)
{
	eg::SpecializationConstantEntry specConstants[] = { { 0, 0, sizeof(uint32_t) } };
	
	const auto& depthBlurTwoPassFS = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/DepthBlur2P.fs.glsl");
	eg::GraphicsPipelineCreateInfo pipelineBlurCI;
	pipelineBlurCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").DefaultVariant();
	pipelineBlurCI.fragmentShader = depthBlurTwoPassFS.GetVariant("V1");
	pipelineBlurCI.fragmentShader.specConstants = specConstants;
	pipelineBlurCI.fragmentShader.specConstantsData = &samples;
	pipelineBlurCI.fragmentShader.specConstantsDataSize = sizeof(uint32_t);
	m_pipelineBlurPass1 = eg::Pipeline::Create(pipelineBlurCI);
	
	pipelineBlurCI.fragmentShader.shaderModule = depthBlurTwoPassFS.GetVariant("V2");
	m_pipelineBlurPass2 = eg::Pipeline::Create(pipelineBlurCI);
	
	pipelineBlurCI.fragmentShader.shaderModule = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/DepthBlur1P.fs.glsl").DefaultVariant();
	m_pipelineBlurSinglePass = eg::Pipeline::Create(pipelineBlurCI);
}

void WaterRenderer::RenderBasic(eg::BufferRef positionsBuffer, uint32_t numParticles) const
{
	eg::DC.BindPipeline(m_pipelineBasic);
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	
	eg::DC.BindVertexBuffer(0, m_quadVB, 0);
	eg::DC.BindVertexBuffer(1, positionsBuffer, 0);
	eg::DC.Draw(0, 4, 0, numParticles);
}

#pragma pack(push, 1)
struct WaterBlurPC
{
	glm::vec2 blurDir;
	float blurDistanceFalloff;
	float blurDepthFalloff;
};
#pragma pack(pop)

static float* blurRadius          = eg::TweakVarFloat("wblur_radius", 150.0f, 0.0f);
static float* blurDistanceFalloff = eg::TweakVarFloat("wblur_distfall", 0.5f, 0.0f);
static float* blurDepthFalloff    = eg::TweakVarFloat("wblur_depthfall", 0.01f, 0.0f);

/*
Water quality settings:
vlow:  4 samples, 16-bit, Half-res, LQ-Shader
low:   8 samples, 16-bit, Half-res, SQ-Shader
med:   8 samples, 32-bit, Half-res, SQ-Shader
high:  16 samples, 32-bit, Half-res, HQ-Shader
vhigh: 16 samples, 32-bit, Half-res, HQ-Shader
*/

static constexpr QualityLevel highPrecisionMinQL = QualityLevel::Medium;
static constexpr QualityLevel fullResolutionMinQL = QualityLevel::High;

static const int blurSamplesByQuality[] = { 4, 8, 8, 16, 16 };

void WaterRenderer::Render(eg::BufferRef positionsBuffer, uint32_t numParticles, RenderTarget& renderTarget)
{
	if (m_currentQualityLevel != settings.waterQuality)
	{
		m_currentQualityLevel = settings.waterQuality;
		CreateDepthBlurPipelines(blurSamplesByQuality[(int)m_currentQualityLevel]);
	}
	
	int blurSamples = blurSamplesByQuality[(int)m_currentQualityLevel];
	float relBlurRad = *blurRadius / blurSamples;
	float relDistanceFalloff = *blurDistanceFalloff / blurSamples;
	
	if (settings.waterQuality == QualityLevel::VeryHigh)
	{
		//We can increase blur radius since we use single pass blurring
		relBlurRad *= 1.4f;
	}
	
	// ** First pass: Depth only **
	
	eg::RenderPassBeginInfo depthOnlyRPBeginInfo;
	depthOnlyRPBeginInfo.framebuffer = renderTarget.m_depthPassFramebuffer.handle;
	depthOnlyRPBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	depthOnlyRPBeginInfo.depthClearValue = 1;
	eg::DC.BeginRenderPass(depthOnlyRPBeginInfo);
	
	eg::DC.BindPipeline(m_pipelineDepthOnly);
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	
	eg::DC.BindVertexBuffer(0, m_quadVB, 0);
	eg::DC.BindVertexBuffer(1, positionsBuffer, 0);
	eg::DC.Draw(0, 4, 0, numParticles);
	
	eg::DC.EndRenderPass();
	
	renderTarget.m_depthTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	
	
	// ** Travel depth additive pass **
	
	eg::RenderPassBeginInfo additiveRPBeginInfo;
	additiveRPBeginInfo.framebuffer = renderTarget.m_travelDepthPassFramebuffer.handle;
	additiveRPBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	additiveRPBeginInfo.colorAttachments[0].clearValue = eg::ColorLin(0, 0, 0, 0);
	eg::DC.BeginRenderPass(additiveRPBeginInfo);
	
	eg::DC.BindPipeline(m_pipelineAdditive);
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	
	eg::DC.BindVertexBuffer(0, m_quadVB, 0);
	eg::DC.BindVertexBuffer(1, positionsBuffer, 0);
	eg::DC.Draw(0, 4, 0, numParticles);
	
	eg::DC.EndRenderPass();
	
	renderTarget.m_travelDepthTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	
	if (settings.waterQuality == QualityLevel::VeryHigh)
	{
		// ** Single pass depth blur **
		eg::RenderPassBeginInfo depthBlurBeginInfo;
		depthBlurBeginInfo.framebuffer = renderTarget.m_blurredDepthFramebuffer2.handle;
		depthBlurBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Discard;
		eg::DC.BeginRenderPass(depthBlurBeginInfo);
		
		WaterBlurPC blurPC;
		blurPC.blurDir = glm::vec2(relBlurRad / renderTarget.m_width, relBlurRad / renderTarget.m_height);
		blurPC.blurDepthFalloff = *blurDepthFalloff;
		blurPC.blurDistanceFalloff = relDistanceFalloff;
		
		eg::DC.BindPipeline(m_pipelineBlurSinglePass);
		eg::DC.PushConstants(0, blurPC);
		
		eg::DC.BindTexture(renderTarget.m_depthTexture, 0, 0);
		eg::DC.BindTexture(renderTarget.m_travelDepthTexture, 0, 1);
		eg::DC.Draw(0, 3, 0, 1);
		
		eg::DC.EndRenderPass();
	}
	else
	{
		// ** Depth blur pass 1 **
		eg::RenderPassBeginInfo depthBlur1RPBeginInfo;
		depthBlur1RPBeginInfo.framebuffer = renderTarget.m_blurredDepthFramebuffer1.handle;
		depthBlur1RPBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Discard;
		eg::DC.BeginRenderPass(depthBlur1RPBeginInfo);
		
		WaterBlurPC blurPC;
		blurPC.blurDir = glm::vec2(0, relBlurRad / renderTarget.m_height);
		blurPC.blurDepthFalloff = *blurDepthFalloff;
		blurPC.blurDistanceFalloff = relDistanceFalloff;
		
		eg::DC.BindPipeline(m_pipelineBlurPass1);
		eg::DC.PushConstants(0, blurPC);
		
		eg::DC.BindTexture(renderTarget.m_depthTexture, 0, 0);
		eg::DC.BindTexture(renderTarget.m_travelDepthTexture, 0, 1);
		eg::DC.Draw(0, 3, 0, 1);
		
		eg::DC.EndRenderPass();
		
		renderTarget.m_blurredDepthTexture1.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
		
		// ** Depth blur pass 2 **
		eg::RenderPassBeginInfo depthBlurBeginInfo;
		depthBlurBeginInfo.framebuffer = renderTarget.m_blurredDepthFramebuffer2.handle;
		depthBlurBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Discard;
		eg::DC.BeginRenderPass(depthBlurBeginInfo);
		
		blurPC.blurDir = glm::vec2(relBlurRad / renderTarget.m_width, 0);
		blurPC.blurDepthFalloff = *blurDepthFalloff;
		blurPC.blurDistanceFalloff = relDistanceFalloff;
		
		eg::DC.BindPipeline(m_pipelineBlurPass2);
		eg::DC.PushConstants(0, blurPC);
		
		eg::DC.BindTexture(renderTarget.m_blurredDepthTexture1, 0, 0);
		eg::DC.Draw(0, 3, 0, 1);
		
		eg::DC.EndRenderPass();
	}
	
	renderTarget.m_blurredDepthTexture2.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	
	// ** Post pass **
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = renderTarget.m_outputFramebuffer.handle;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.colorAttachments[0].clearValue = eg::ColorSRGB::FromHex(0x0c2b46);
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	eg::DC.BindPipeline(m_pipelinesRefPost[(int)settings.waterQuality]);
	
	float pc[2] = { 2.0f / renderTarget.m_width, 2.0f / renderTarget.m_height };
	eg::DC.PushConstants(0, sizeof(pc), pc);
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	eg::DC.BindTexture(renderTarget.m_blurredDepthTexture2, 0, 1);
	eg::DC.BindTexture(renderTarget.m_inputColor, 0, 2);
	eg::DC.BindTexture(renderTarget.m_inputDepth, 0, 3);
	if (settings.waterQuality > QualityLevel::VeryLow)
	{
		eg::DC.BindTexture(*m_normalMapTexture, 0, 4);
	}
	
	eg::DC.Draw(0, 3, 0, 1);
	
	eg::DC.EndRenderPass();
}

WaterRenderer::RenderTarget::RenderTarget(uint32_t width, uint32_t height, eg::TextureRef inputColor,
	eg::TextureRef inputDepth, eg::TextureRef outputTexture, uint32_t outputArrayLayer, QualityLevel waterQuality)
	: m_width(width / 2), m_height(height / 2), m_inputColor(inputColor), m_inputDepth(inputDepth), m_outputTexture(outputTexture)
{
	bool highPrecision = waterQuality >= highPrecisionMinQL;
	
	eg::SamplerDescription fbSamplerDesc;
	fbSamplerDesc.wrapU = eg::WrapMode::ClampToEdge;
	fbSamplerDesc.wrapV = eg::WrapMode::ClampToEdge;
	fbSamplerDesc.wrapW = eg::WrapMode::ClampToEdge;
	fbSamplerDesc.minFilter = eg::TextureFilter::Linear;
	fbSamplerDesc.magFilter = eg::TextureFilter::Linear;
	
	eg::TextureCreateInfo depthTextureCI;
	depthTextureCI.format = eg::Format::Depth32;
	depthTextureCI.width = m_width;
	depthTextureCI.height = m_height;
	depthTextureCI.mipLevels = 1;
	depthTextureCI.flags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample;
	depthTextureCI.defaultSamplerDescription = &fbSamplerDesc;
	m_depthTexture = eg::Texture::Create2D(depthTextureCI);
	
	m_depthPassFramebuffer = eg::Framebuffer({}, m_depthTexture.handle);
	
	
	eg::TextureCreateInfo travelDepthTextureCI;
	travelDepthTextureCI.format = highPrecision ? eg::Format::R32_Float : eg::Format::R16_Float;
	travelDepthTextureCI.width = m_width;
	travelDepthTextureCI.height = m_height;
	travelDepthTextureCI.mipLevels = 1;
	travelDepthTextureCI.flags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample;
	travelDepthTextureCI.defaultSamplerDescription = &fbSamplerDesc;
	m_travelDepthTexture = eg::Texture::Create2D(travelDepthTextureCI);
	
	eg::FramebufferAttachment travelDepthAttachments[] = { m_travelDepthTexture.handle };
	m_travelDepthPassFramebuffer = eg::Framebuffer(travelDepthAttachments);
	
	
	eg::TextureCreateInfo blurTextureCI;
	blurTextureCI.format = highPrecision ? eg::Format::R32G32_Float : eg::Format::R16G16_Float;
	blurTextureCI.width = m_width;
	blurTextureCI.height = m_height;
	blurTextureCI.mipLevels = 1;
	blurTextureCI.flags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample;
	blurTextureCI.defaultSamplerDescription = &fbSamplerDesc;
	m_blurredDepthTexture1 = eg::Texture::Create2D(blurTextureCI);
	m_blurredDepthTexture2 = eg::Texture::Create2D(blurTextureCI);
	
	eg::FramebufferAttachment blurredDepthAttachments1[] = { m_blurredDepthTexture1.handle };
	eg::FramebufferAttachment blurredDepthAttachments2[] = { m_blurredDepthTexture2.handle };
	
	m_blurredDepthFramebuffer1 = eg::Framebuffer(blurredDepthAttachments1);
	m_blurredDepthFramebuffer2 = eg::Framebuffer(blurredDepthAttachments2);
	
	eg::FramebufferAttachment outputAttachments[1];
	outputAttachments[0].texture = outputTexture.handle;
	outputAttachments[0].subresource.firstArrayLayer = outputArrayLayer;
	m_outputFramebuffer = eg::Framebuffer(outputAttachments);
}
