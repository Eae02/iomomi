#include "WaterRenderer.hpp"
#include "../RenderSettings.hpp"
#include "../RenderTex.hpp"
#include "../../Settings.hpp"

static eg::Texture dummyWaterDepthTexture;

static void CreateDummyDepthTexture()
{
	eg::SamplerDescription dummyDepthTextureSamplerDesc; 
	eg::TextureCreateInfo dummyDepthTextureCI;
	dummyDepthTextureCI.width = 1;
	dummyDepthTextureCI.height = 1;
	dummyDepthTextureCI.format = eg::Format::R32G32B32A32_Float;
	dummyDepthTextureCI.mipLevels = 1;
	dummyDepthTextureCI.flags = eg::TextureFlags::CopyDst | eg::TextureFlags::ShaderSample;
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
	
	auto& sphereDepthFS = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/WaterSphere.fs.glsl");
	auto& sphereVS = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/WaterSphere.vs.glsl");
	
	eg::GraphicsPipelineCreateInfo pipelineCITemplate;
	pipelineCITemplate.topology = eg::Topology::TriangleStrip;
	pipelineCITemplate.vertexAttributes[0] = { 0, eg::DataType::Float32, 2, 0 };
	//pipelineCITemplate.vertexAttributes[1] = { 1, eg::DataType::Float32, 4, 0 };
	pipelineCITemplate.vertexBindings[0] = { sizeof(float) * 2, eg::InputRate::Vertex };
	//pipelineCITemplate.vertexBindings[1] = { sizeof(float) * 4, eg::InputRate::Instance };
	
	eg::GraphicsPipelineCreateInfo pipelineDepthMinCI = pipelineCITemplate;
	pipelineDepthMinCI.vertexShader = sphereVS.GetVariant("VDepthMin");
	pipelineDepthMinCI.fragmentShader = sphereDepthFS.GetVariant("VDepthMin");
	pipelineDepthMinCI.enableDepthTest = true;
	pipelineDepthMinCI.enableDepthWrite = true;
	pipelineDepthMinCI.enableDepthClamp = true;
	pipelineDepthMinCI.depthCompare = eg::CompareOp::Less;
	m_pipelineDepthMin = eg::Pipeline::Create(pipelineDepthMinCI);
	
	eg::GraphicsPipelineCreateInfo pipelineDepthMaxCI = pipelineCITemplate;
	pipelineDepthMaxCI.vertexShader = sphereVS.GetVariant("VDepthMax");
	pipelineDepthMaxCI.fragmentShader = sphereDepthFS.GetVariant("VDepthMax");
	pipelineDepthMaxCI.enableDepthTest = true;
	pipelineDepthMaxCI.enableDepthWrite = true;
	pipelineDepthMaxCI.enableDepthClamp = true;
	pipelineDepthMaxCI.depthCompare = eg::CompareOp::Greater;
	m_pipelineDepthMax = eg::Pipeline::Create(pipelineDepthMaxCI);
	
	auto& postFS = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/WaterPost.fs.glsl");
	eg::GraphicsPipelineCreateInfo pipelinePostCI;
	pipelinePostCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").DefaultVariant();
	
	pipelinePostCI.fragmentShader = postFS.GetVariant("VStdQual");
	m_pipelinePostStdQual = eg::Pipeline::Create(pipelinePostCI);
	
	pipelinePostCI.fragmentShader = postFS.GetVariant("VHighQual");
	m_pipelinePostHighQual = eg::Pipeline::Create(pipelinePostCI);
	
	m_pipelinesRefPost[(int)QualityLevel::VeryLow]  = m_pipelinePostStdQual;
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
	
	const auto& depthBlurTwoPassFS = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/DepthBlur.fs.glsl");
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
}

#pragma pack(push, 1)
struct WaterBlurPC
{
	glm::vec2 blurDir;
	float blurDistanceFalloff;
	float blurDepthFalloff;
};
#pragma pack(pop)

static float* blurRadius          = eg::TweakVarFloat("wblur_radius", 1.0f, 0.0f);
static float* blurDistanceFalloff = eg::TweakVarFloat("wblur_distfall", 0.5f, 0.0f);
static float* blurDepthFalloff    = eg::TweakVarFloat("wblur_depthfall", 0.2f, 0.0f);

/*
Water quality settings:
vlow:  6 samples,  16-bit, SQ-Shader
low:   10 samples, 16-bit, SQ-Shader
med:   10 samples, 32-bit, SQ-Shader
high:  16 samples, 32-bit, HQ-Shader
vhigh: 26 samples, 32-bit, HQ-Shader
*/

static const int blurSamplesByQuality[] = { 6, 10, 10, 16, 24 };
static const float baseBlurRadius[] = { 60.0f, 80.0f, 100.0f, 120.0f, 150.0f };

void WaterRenderer::RenderEarly(eg::BufferRef positionsBuffer, uint32_t numParticles, RenderTexManager& rtManager)
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
	depthMinRPBeginInfo.framebuffer = rtManager.GetFramebuffer(RenderTex::WaterGlowIntensity, {}, RenderTex::WaterMinDepth, "WaterMinDepth");
	depthMinRPBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	depthMinRPBeginInfo.depthClearValue = 1;
	depthMinRPBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	depthMinRPBeginInfo.colorAttachments[0].clearValue = eg::ColorLin(0, 0, 0, 0);
	eg::DC.BeginRenderPass(depthMinRPBeginInfo);
	
	eg::DC.BindPipeline(m_pipelineDepthMin);
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	
	eg::DC.BindVertexBuffer(0, m_quadVB, 0);
	eg::DC.BindStorageBuffer(positionsBuffer, 0, 1, 0, numParticles * sizeof(float) * 4);
	//eg::DC.BindVertexBuffer(1, positionsBuffer, 0);
	eg::DC.Draw(0, 4, 0, numParticles);
	
	eg::DC.EndRenderPass();
	eg::DC.DebugLabelEnd();
	
	// ** Depth max pass **
	//This pass renders all water particles that are closer to the camera than the the travel depth
	// to a depth buffer. Greater compare mode is used to get the maximum depth bounds.
	// This is used to render the water surface from below.
	timer.StartStage("Depth Max");
	eg::DC.DebugLabelBegin("Depth Max");
	
	eg::RenderPassBeginInfo depthMaxRPBeginInfo;
	depthMaxRPBeginInfo.framebuffer = rtManager.GetFramebuffer({}, {}, RenderTex::WaterMaxDepth, "WaterMaxDepth");
	depthMaxRPBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	depthMaxRPBeginInfo.depthClearValue = 0;
	eg::DC.BeginRenderPass(depthMaxRPBeginInfo);
	
	eg::DC.BindPipeline(m_pipelineDepthMax);
	
	uint32_t depthMaxPC = numParticles - 1;
	eg::DC.PushConstants(0, sizeof(uint32_t), &depthMaxPC);
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	eg::DC.BindStorageBuffer(positionsBuffer, 0, 1, 0, numParticles * sizeof(float) * 4);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::GBDepth), 0, 2);
	
	eg::DC.BindVertexBuffer(0, m_quadVB, 0);
	//eg::DC.BindVertexBuffer(1, positionsBuffer, 0);
	eg::DC.Draw(0, 4, 0, numParticles);
	
	eg::DC.EndRenderPass();
	eg::DC.DebugLabelEnd();
	
	rtManager.RenderTextureUsageHintFS(RenderTex::WaterMinDepth);
	rtManager.RenderTextureUsageHintFS(RenderTex::WaterMaxDepth);
	
	
	timer.StartStage("Blur");
	eg::DC.DebugLabelBegin("Blur");
	
	// ** Depth blur pass 1 **
	eg::RenderPassBeginInfo depthBlur1RPBeginInfo;
	depthBlur1RPBeginInfo.framebuffer = rtManager.GetFramebuffer(RenderTex::WaterDepthBlurred1, {}, {}, "WaterDepthBlurred1");
	depthBlur1RPBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	eg::DC.BeginRenderPass(depthBlur1RPBeginInfo);
	
	WaterBlurPC blurPC;
	blurPC.blurDir = glm::vec2(0, relBlurRad / rtManager.ResY());
	blurPC.blurDepthFalloff = *blurDepthFalloff;
	blurPC.blurDistanceFalloff = relDistanceFalloff;
	
	eg::DC.BindPipeline(m_pipelineBlurPass1);
	eg::DC.PushConstants(0, blurPC);
	
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::WaterMinDepth), 0, 0);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::WaterMaxDepth), 0, 1);
	eg::DC.Draw(0, 3, 0, 1);
	
	eg::DC.EndRenderPass();
	
	rtManager.RenderTextureUsageHintFS(RenderTex::WaterDepthBlurred1);
	
	// ** Depth blur pass 2 **
	eg::RenderPassBeginInfo depthBlurBeginInfo;
	depthBlurBeginInfo.framebuffer = rtManager.GetFramebuffer(RenderTex::WaterDepthBlurred2, {}, {}, "WaterDepthBlurred2");
	depthBlurBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	eg::DC.BeginRenderPass(depthBlurBeginInfo);
	
	blurPC.blurDir = glm::vec2(relBlurRad / rtManager.ResX(), 0);
	blurPC.blurDepthFalloff = *blurDepthFalloff;
	blurPC.blurDistanceFalloff = relDistanceFalloff;
	
	eg::DC.BindPipeline(m_pipelineBlurPass2);
	eg::DC.PushConstants(0, blurPC);
	
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::WaterDepthBlurred1), 0, 0);
	eg::DC.Draw(0, 3, 0, 1);
	
	eg::DC.EndRenderPass();
	
	eg::DC.DebugLabelEnd();
	
	rtManager.RenderTextureUsageHintFS(RenderTex::WaterDepthBlurred2);
	rtManager.RenderTextureUsageHintFS(RenderTex::WaterGlowIntensity);
}

static const glm::vec3 waterGlowColor(0.12f, 0.9f, 0.7f);

static float* waterVisibility = eg::TweakVarFloat("water_visibility", 10.0f, 0.0f);
static float* waterNormalMapIntensity = eg::TweakVarFloat("water_nm_intensity", 0.6f, 0.0f);
static float* waterSSRIntensity = eg::TweakVarFloat("water_ssr_intensity", 2.0f, 0.0f);
static float* waterIndexOfRefraction = eg::TweakVarFloat("water_ior", 0.8f);

void WaterRenderer::RenderPost(RenderTexManager& rtManager)
{
	// ** Post pass **
	
	eg::GPUTimer timer = eg::StartGPUTimer("Water Post");
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = rtManager.GetFramebuffer(RenderTex::LitWithoutSSR, {}, {}, "WaterPost");
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.colorAttachments[0].clearValue = eg::ColorSRGB::FromHex(0x0c2b46);
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	eg::DC.BindPipeline(m_pipelinesRefPost[(int)settings.waterQuality]);
	
	float waterGlowIntensity = settings.gunFlash ? 4 : 0.5f;
	
	float pc[] =
	{
		2.0f / rtManager.ResX(), 2.0f / rtManager.ResY(),
		*waterVisibility,
		*waterNormalMapIntensity,
		waterGlowColor.r * waterGlowIntensity,
		waterGlowColor.g * waterGlowIntensity,
		waterGlowColor.b * waterGlowIntensity,
		*waterSSRIntensity,
		*waterIndexOfRefraction
	};
	eg::DC.PushConstants(0, sizeof(pc), pc);
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::WaterDepthBlurred2), 0, 1);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::WaterMaxDepth), 0, 2);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::WaterGlowIntensity), 0, 3);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::LitWithoutWater), 0, 4);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::GBDepth), 0, 5);
	eg::DC.BindTexture(*m_normalMapTexture, 0, 6);
	
	eg::DC.Draw(0, 3, 0, 1);
	
	eg::DC.EndRenderPass();
}
