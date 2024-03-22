#include "ParticleRenderer.hpp"

#include "GraphicsCommon.hpp"
#include "RenderSettings.hpp"
#include "RenderTargets.hpp"

static eg::Pipeline particlePipeline;
static eg::Buffer particleVertexBuffer;

static void OnInit()
{
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Particle.vs.glsl").ToStageInfo();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Particle.fs.glsl").ToStageInfo();
	pipelineCI.topology = eg::Topology::TriangleStrip;
	pipelineCI.enableDepthTest = true;
	pipelineCI.enableDepthWrite = false;
	pipelineCI.numColorAttachments = 1;
	pipelineCI.blendStates[0] = { eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::OneMinusSrcAlpha };
	pipelineCI.vertexBindings[0] = { sizeof(float) * 2, eg::InputRate::Vertex };
	pipelineCI.vertexBindings[1] = { sizeof(eg::ParticleInstance), eg::InputRate::Instance };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 2, 0 };
	pipelineCI.vertexAttributes[1] = { 1, eg::DataType::Float32, 4, offsetof(eg::ParticleInstance, position) };
	pipelineCI.vertexAttributes[2] = { 1, eg::DataType::UInt16Norm, 4, offsetof(eg::ParticleInstance, texCoord) };
	pipelineCI.vertexAttributes[3] = { 1, eg::DataType::UInt8Norm, 4, offsetof(eg::ParticleInstance, sinR) };
	pipelineCI.colorAttachmentFormats[0] = lightColorAttachmentFormat;
	pipelineCI.depthAttachmentFormat = GB_DEPTH_FORMAT;
	pipelineCI.depthStencilUsage = eg::TextureUsage::DepthStencilReadOnly;
	pipelineCI.label = "Particle";
	particlePipeline = eg::Pipeline::Create(pipelineCI);
}

static void OnShutdown()
{
	particlePipeline.Destroy();
	particleVertexBuffer.Destroy();
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

ParticleRenderer::ParticleRenderer()
{
	if (particleVertexBuffer.handle == nullptr)
	{
		const float vertices[] = { 0, 0, 1, 0, 0, 1, 1, 1 };
		particleVertexBuffer = eg::Buffer(eg::BufferFlags::VertexBuffer, sizeof(vertices), vertices);
		particleVertexBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
	}

	m_descriptorSet0 = eg::DescriptorSet(particlePipeline, 0);
	m_descriptorSet0.BindUniformBuffer(RenderSettings::instance->Buffer(), 0);
	m_descriptorSet0.BindTexture(eg::GetAsset<eg::Texture>("Textures/Particles.png"), 1);
	m_descriptorSet0.BindSampler(samplers::linearClampAnisotropic, 2);
}

void ParticleRenderer::Draw(const eg::ParticleManager& particleManager)
{
	if (particleManager.ParticlesToDraw() == 0)
		return;

	auto gpuTimerTransparent = eg::StartGPUTimer("Particles");
	auto cpuTimerTransparent = eg::StartCPUTimer("Particles");

	eg::DC.BindPipeline(particlePipeline);
	eg::DC.BindDescriptorSet(m_descriptorSet0, 0);
	eg::DC.BindDescriptorSet(m_descriptorSet1, 1);

	eg::DC.BindVertexBuffer(0, particleVertexBuffer, 0);
	eg::DC.BindVertexBuffer(1, particleManager.ParticlesBuffer(), 0);

	eg::DC.Draw(0, 4, 0, particleManager.ParticlesToDraw());
}

void ParticleRenderer::SetDepthTexture(eg::TextureRef depthTexture)
{
	m_descriptorSet1 = eg::DescriptorSet(particlePipeline, 1);
	m_descriptorSet1.BindTexture(depthTexture, 0, {}, eg::TextureUsage::DepthStencilReadOnly);
}
