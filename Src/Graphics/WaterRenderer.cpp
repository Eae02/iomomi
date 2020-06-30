#include "WaterRenderer.hpp"
#include "RenderSettings.hpp"
#include "GraphicsCommon.hpp"
#include "../Settings.hpp"

static eg::Texture dummyWaterDepthTexture;

static void CreateDummyDepthTexture()
{
	eg::SamplerDescription dummyDepthTextureSamplerDesc; 
	eg::TextureCreateInfo dummyDepthTextureCI;
	dummyDepthTextureCI.width = 1;
	dummyDepthTextureCI.height = 1;
	dummyDepthTextureCI.format = eg::Format::R32G32B32A32_Float;
	dummyDepthTextureCI.mipLevels = 1;
	dummyDepthTextureCI.flags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample;
	dummyDepthTextureCI.defaultSamplerDescription = &dummyDepthTextureSamplerDesc;
	
	dummyWaterDepthTexture = eg::Texture::Create2D(dummyDepthTextureCI);
	
	eg::DC.ClearColorTexture(dummyWaterDepthTexture, 0, eg::Color(1000, 1000, 1000, 1));
	dummyWaterDepthTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
}

static void DestroyDummyDepthTexture()
{
	dummyWaterDepthTexture.Destroy();
}

EG_ON_INIT(CreateDummyDepthTexture)
EG_ON_SHUTDOWN(DestroyDummyDepthTexture)

eg::TextureRef WaterRenderer::GetDummyDepthTexture()
{
	return dummyWaterDepthTexture;
}

WaterRenderer::WaterRenderer()
{
	float quadVBData[] = { -1, -1, -1, 1, 1, -1, 1, 1 };
	m_quadVB = eg::Buffer(eg::BufferFlags::VertexBuffer, sizeof(quadVBData), quadVBData);
	m_quadVB.UsageHint(eg::BufferUsage::VertexBuffer);
	
	eg::GraphicsPipelineCreateInfo pipelineCITemplate;
	pipelineCITemplate.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/WaterSphere.vs.glsl").DefaultVariant();
	pipelineCITemplate.topology = eg::Topology::TriangleStrip;
	pipelineCITemplate.vertexAttributes[0] = { 0, eg::DataType::Float32, 2, 0 };
	pipelineCITemplate.vertexAttributes[1] = { 1, eg::DataType::Float32, 3, 0 };
	pipelineCITemplate.vertexBindings[0] = { sizeof(float) * 2, eg::InputRate::Vertex };
	pipelineCITemplate.vertexBindings[1] = { sizeof(float) * 4, eg::InputRate::Instance };
	
	eg::GraphicsPipelineCreateInfo pipelineBasicCI = pipelineCITemplate;
	pipelineBasicCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/WaterSphereBasic.fs.glsl").DefaultVariant();
	pipelineBasicCI.enableDepthTest = true;
	pipelineBasicCI.enableDepthWrite = true;
	m_pipelineBasic = eg::Pipeline::Create(pipelineBasicCI);
	
	auto& sphereDepthFS = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/WaterSphereDepth.fs.glsl");
	
	eg::GraphicsPipelineCreateInfo pipelineDepthMinCI = pipelineCITemplate;
	pipelineDepthMinCI.fragmentShader = sphereDepthFS.GetVariant("VDepthMin");
	pipelineDepthMinCI.enableDepthTest = true;
	pipelineDepthMinCI.enableDepthWrite = true;
	m_pipelineDepthMin = eg::Pipeline::Create(pipelineDepthMinCI);
	
	eg::GraphicsPipelineCreateInfo pipelineDepthMaxCI = pipelineCITemplate;
	pipelineDepthMaxCI.fragmentShader = sphereDepthFS.GetVariant("VDepthMax");
	pipelineDepthMaxCI.enableDepthTest = true;
	pipelineDepthMaxCI.enableDepthWrite = true;
	pipelineDepthMaxCI.depthCompare = eg::CompareOp::Greater;
	m_pipelineDepthMax = eg::Pipeline::Create(pipelineDepthMaxCI);
	
	eg::GraphicsPipelineCreateInfo pipelineAddCI = pipelineCITemplate;
	pipelineAddCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/WaterSphereAdd.fs.glsl").DefaultVariant();
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
	m_pipelineBlurPass1.FramebufferFormatHint(eg::Format::R32G32B32A32_Float);
	m_pipelineBlurPass1.FramebufferFormatHint(eg::Format::R16G16B16A16_Float);
	
	pipelineBlurCI.fragmentShader.shaderModule = depthBlurTwoPassFS.GetVariant("V2");
	m_pipelineBlurPass2 = eg::Pipeline::Create(pipelineBlurCI);
	m_pipelineBlurPass2.FramebufferFormatHint(eg::Format::R32G32B32A32_Float);
	m_pipelineBlurPass2.FramebufferFormatHint(eg::Format::R16G16B16A16_Float);
	
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

static float* blurRadius          = eg::TweakVarFloat("wblur_radius", 2.0f, 0.0f);
static float* blurDistanceFalloff = eg::TweakVarFloat("wblur_distfall", 0.5f, 0.0f);
static float* blurDepthFalloff    = eg::TweakVarFloat("wblur_depthfall", 0.01f, 0.0f);
static int* blurSinglePass        = eg::TweakVarInt("wblur_singlepass", 0, 0, 1);

/*
Water quality settings:
vlow:  4 samples, 16-bit, LQ-Shader
low:   8 samples, 16-bit, SQ-Shader
med:   16 samples, 32-bit, SQ-Shader
high:  32 samples, 32-bit, HQ-Shader
vhigh: 48 samples, 32-bit, HQ-Shader
*/



static const int blurSamplesByQuality[] = { 4, 8, 16, 24, 32 };
static const float baseBlurRadius[] = { 70.0f, 100.0f, 125.0f, 150.0f, 150.0f };

void WaterRenderer::RenderEarly(eg::BufferRef positionsBuffer, uint32_t numParticles)
{
	if (m_currentQualityLevel != settings.waterQuality)
	{
		m_currentQualityLevel = settings.waterQuality;
		CreateDepthBlurPipelines(blurSamplesByQuality[(int)m_currentQualityLevel]);
	}
	
	int blurSamples = blurSamplesByQuality[(int)m_currentQualityLevel];
	float relBlurRad = (*blurRadius * baseBlurRadius[(int)m_currentQualityLevel]) / blurSamples;
	float relDistanceFalloff = *blurDistanceFalloff / blurSamples;
	
	// ** Depth min pass **
	//This pass renders all water particles to a depth buffer
	// using less compare mode to get the minimum depth bounds.
	
	eg::MultiStageGPUTimer timer;
	timer.StartStage("Depth");
	eg::DC.DebugLabelBegin("Depth");
	
	eg::RenderPassBeginInfo depthMinRPBeginInfo;
	depthMinRPBeginInfo.framebuffer = GetFramebuffer({}, {}, RenderTex::WaterMinDepth, "WaterMinDepth");
	depthMinRPBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	depthMinRPBeginInfo.depthClearValue = 1;
	eg::DC.BeginRenderPass(depthMinRPBeginInfo);
	
	eg::DC.BindPipeline(m_pipelineDepthMin);
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	
	eg::DC.BindVertexBuffer(0, m_quadVB, 0);
	eg::DC.BindVertexBuffer(1, positionsBuffer, 0);
	eg::DC.Draw(0, 4, 0, numParticles);
	
	eg::DC.EndRenderPass();
	eg::DC.DebugLabelEnd();
	
	RenderTextureUsageHintFS(RenderTex::WaterMinDepth);
	
	
	// ** Travel depth additive pass **
	//This pass renders all water particles additively to get the water travel depth.
	
	timer.StartStage("Additive Depth");
	eg::DC.DebugLabelBegin("Additive Depth");
	
	eg::RenderPassBeginInfo additiveRPBeginInfo;
	additiveRPBeginInfo.framebuffer = GetFramebuffer(RenderTex::WaterTravelDepth, {}, {}, "WaterTravelDepth");
	additiveRPBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	additiveRPBeginInfo.colorAttachments[0].clearValue = eg::ColorLin(0, 0, 0, 0);
	eg::DC.BeginRenderPass(additiveRPBeginInfo);
	
	eg::DC.BindPipeline(m_pipelineAdditive);
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	
	eg::DC.BindVertexBuffer(0, m_quadVB, 0);
	eg::DC.BindVertexBuffer(1, positionsBuffer, 0);
	eg::DC.Draw(0, 4, 0, numParticles);
	
	eg::DC.EndRenderPass();
	eg::DC.DebugLabelEnd();
	
	RenderTextureUsageHintFS(RenderTex::WaterTravelDepth);
	
	// ** Depth max pass **
	//This pass renders all water particles that are closer to the camera than the the travel depth
	// to a depth buffer. Greater compare mode is used to get the maximum depth bounds.
	// This is used to render the water surface from below.
	timer.StartStage("Depth Max");
	eg::DC.DebugLabelBegin("Depth Max");
	
	eg::RenderPassBeginInfo depthMaxRPBeginInfo;
	depthMaxRPBeginInfo.framebuffer = GetFramebuffer({}, {}, RenderTex::WaterMaxDepth, "WaterMaxDepth");
	depthMaxRPBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	depthMaxRPBeginInfo.depthClearValue = 0;
	eg::DC.BeginRenderPass(depthMaxRPBeginInfo);
	
	eg::DC.BindPipeline(m_pipelineDepthMax);
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	eg::DC.BindTexture(GetRenderTexture(RenderTex::GBDepth), 0, 1);
	
	eg::DC.BindVertexBuffer(0, m_quadVB, 0);
	eg::DC.BindVertexBuffer(1, positionsBuffer, 0);
	eg::DC.Draw(0, 4, 0, numParticles);
	
	eg::DC.EndRenderPass();
	eg::DC.DebugLabelEnd();
	
	RenderTextureUsageHintFS(RenderTex::WaterMaxDepth);
	
	
	timer.StartStage("Blur");
	eg::DC.DebugLabelBegin("Blur");
	
	if (*blurSinglePass)
	{
		// ** Single pass depth blur **
		eg::RenderPassBeginInfo depthBlurBeginInfo;
		depthBlurBeginInfo.framebuffer = GetFramebuffer(RenderTex::WaterDepthBlurred2, {}, {}, "WaterDepthBlurred2");
		depthBlurBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
		eg::DC.BeginRenderPass(depthBlurBeginInfo);
		
		WaterBlurPC blurPC;
		blurPC.blurDir = glm::vec2(relBlurRad / eg::CurrentResolutionX(), relBlurRad / eg::CurrentResolutionY());
		blurPC.blurDepthFalloff = *blurDepthFalloff;
		blurPC.blurDistanceFalloff = relDistanceFalloff;
		
		eg::DC.BindPipeline(m_pipelineBlurSinglePass);
		eg::DC.PushConstants(0, blurPC);
		
		eg::DC.BindTexture(GetRenderTexture(RenderTex::WaterMinDepth), 0, 0);
		eg::DC.BindTexture(GetRenderTexture(RenderTex::WaterTravelDepth), 0, 1);
		eg::DC.Draw(0, 3, 0, 1);
		
		eg::DC.EndRenderPass();
	}
	else
	{
		// ** Depth blur pass 1 **
		eg::RenderPassBeginInfo depthBlur1RPBeginInfo;
		depthBlur1RPBeginInfo.framebuffer = GetFramebuffer(RenderTex::WaterDepthBlurred1, {}, {}, "WaterDepthBlurred1");
		depthBlur1RPBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
		eg::DC.BeginRenderPass(depthBlur1RPBeginInfo);
		
		WaterBlurPC blurPC;
		blurPC.blurDir = glm::vec2(0, relBlurRad / eg::CurrentResolutionY());
		blurPC.blurDepthFalloff = *blurDepthFalloff;
		blurPC.blurDistanceFalloff = relDistanceFalloff;
		
		eg::DC.BindPipeline(m_pipelineBlurPass1);
		eg::DC.PushConstants(0, blurPC);
		
		eg::DC.BindTexture(GetRenderTexture(RenderTex::WaterMinDepth), 0, 0);
		eg::DC.BindTexture(GetRenderTexture(RenderTex::WaterTravelDepth), 0, 1);
		eg::DC.BindTexture(GetRenderTexture(RenderTex::WaterMaxDepth), 0, 2);
		eg::DC.Draw(0, 3, 0, 1);
		
		eg::DC.EndRenderPass();
		
		RenderTextureUsageHintFS(RenderTex::WaterDepthBlurred1);
		
		// ** Depth blur pass 2 **
		eg::RenderPassBeginInfo depthBlurBeginInfo;
		depthBlurBeginInfo.framebuffer = GetFramebuffer(RenderTex::WaterDepthBlurred2, {}, {}, "WaterDepthBlurred2");
		depthBlurBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
		eg::DC.BeginRenderPass(depthBlurBeginInfo);
		
		blurPC.blurDir = glm::vec2(relBlurRad / eg::CurrentResolutionX(), 0);
		blurPC.blurDepthFalloff = *blurDepthFalloff;
		blurPC.blurDistanceFalloff = relDistanceFalloff;
		
		eg::DC.BindPipeline(m_pipelineBlurPass2);
		eg::DC.PushConstants(0, blurPC);
		
		eg::DC.BindTexture(GetRenderTexture(RenderTex::WaterDepthBlurred1), 0, 0);
		eg::DC.Draw(0, 3, 0, 1);
		
		eg::DC.EndRenderPass();
	}
	
	eg::DC.DebugLabelEnd();
	
	RenderTextureUsageHintFS(RenderTex::WaterDepthBlurred2);
}

void WaterRenderer::RenderPost()
{
	// ** Post pass **
	
	eg::GPUTimer timer = eg::StartGPUTimer("Water Post");
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = GetFramebuffer(RenderTex::Lit, {}, {}, "WaterPost");
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.colorAttachments[0].clearValue = eg::ColorSRGB::FromHex(0x0c2b46);
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	eg::DC.BindPipeline(m_pipelinesRefPost[(int)settings.waterQuality]);
	
	float pc[2] = { 2.0f / eg::CurrentResolutionX(), 2.0f / eg::CurrentResolutionY() };
	eg::DC.PushConstants(0, sizeof(pc), pc);
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	eg::DC.BindTexture(GetRenderTexture(RenderTex::WaterDepthBlurred2), 0, 1);
	eg::DC.BindTexture(GetRenderTexture(RenderTex::LitWithoutWater), 0, 2);
	eg::DC.BindTexture(GetRenderTexture(RenderTex::GBDepth), 0, 3);
	if (settings.waterQuality > QualityLevel::VeryLow)
	{
		eg::DC.BindTexture(*m_normalMapTexture, 0, 4);
	}
	
	eg::DC.Draw(0, 3, 0, 1);
	
	eg::DC.EndRenderPass();
}
