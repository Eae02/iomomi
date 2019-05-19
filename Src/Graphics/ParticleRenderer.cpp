#include "ParticleRenderer.hpp"
#include "RenderSettings.hpp"

ParticleRenderer::ParticleRenderer()
{
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Particle.vs.glsl").DefaultVariant();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Particle.fs.glsl").DefaultVariant();
	pipelineCI.topology = eg::Topology::TriangleStrip;
	pipelineCI.numColorAttachments = 1;
	pipelineCI.blendStates[0] = { eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::OneMinusSrcAlpha };
	pipelineCI.vertexBindings[0] = { sizeof(float) * 2, eg::InputRate::Vertex };
	pipelineCI.vertexBindings[1] = { sizeof(eg::ParticleInstance), eg::InputRate::Instance };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 2, 0 };
	pipelineCI.vertexAttributes[1] = { 1, eg::DataType::Float32, 4, offsetof(eg::ParticleInstance, position) };
	pipelineCI.vertexAttributes[2] = { 1, eg::DataType::UInt16Norm, 4, offsetof(eg::ParticleInstance, texCoord) };
	pipelineCI.vertexAttributes[3] = { 1, eg::DataType::UInt8Norm, 4, offsetof(eg::ParticleInstance, sinR) };
	m_pipeline = eg::Pipeline::Create(pipelineCI);
	
	const float vertices[] = { 0, 0, 1, 0, 0, 1, 1, 1 };
	m_vertexBuffer = eg::Buffer(eg::BufferFlags::VertexBuffer, sizeof(vertices), vertices);
	m_vertexBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
	
	m_texture = &eg::GetAsset<eg::Texture>("Textures/Particles.png");
}

void ParticleRenderer::Draw(const eg::ParticleManager& particleManager, eg::TextureRef depthTexture)
{
	if (particleManager.ParticlesToDraw() == 0)
		return;
	
	eg::DC.BindPipeline(m_pipeline);
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	eg::DC.BindTexture(*m_texture, 0, 1);
	eg::DC.BindTexture(depthTexture, 0, 2);
	
	eg::DC.BindVertexBuffer(0, m_vertexBuffer, 0);
	eg::DC.BindVertexBuffer(1, particleManager.ParticlesBuffer(), 0);
	
	eg::DC.Draw(0, 4, 0, particleManager.ParticlesToDraw());
}
