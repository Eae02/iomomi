#include "ForceFieldMaterial.hpp"
#include "MeshDrawArgs.hpp"
#include "../RenderSettings.hpp"

ForceFieldMaterial::ForceFieldMaterial()
{
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/ForceField.vs.glsl").DefaultVariant();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/ForceField.fs.glsl").DefaultVariant();
	pipelineCI.vertexBindings[0] = { sizeof(float) * 2, eg::InputRate::Vertex };
	pipelineCI.vertexBindings[1] = { sizeof(ForceFieldInstance), eg::InputRate::Instance };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 2, 0 };
	pipelineCI.vertexAttributes[1] = { 1, eg::DataType::UInt32, 1, offsetof(ForceFieldInstance, mode) };
	pipelineCI.vertexAttributes[2] = { 1, eg::DataType::Float32, 3, offsetof(ForceFieldInstance, offset) };
	pipelineCI.vertexAttributes[3] = { 1, eg::DataType::Float32, 4, offsetof(ForceFieldInstance, transformX) };
	pipelineCI.vertexAttributes[4] = { 1, eg::DataType::Float32, 4, offsetof(ForceFieldInstance, transformY) };
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	pipelineCI.topology = eg::Topology::TriangleStrip;
	pipelineCI.cullMode = eg::CullMode::None;
	pipelineCI.enableDepthWrite = false;
	pipelineCI.enableDepthTest = true;
	pipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::OneMinusSrcAlpha);
	m_pipeline = eg::Pipeline::Create(pipelineCI);
	
	eg::SamplerDescription particleSamplerDesc;
	particleSamplerDesc.wrapU = eg::WrapMode::ClampToEdge;
	particleSamplerDesc.wrapV = eg::WrapMode::ClampToEdge;
	particleSamplerDesc.wrapW = eg::WrapMode::ClampToEdge;
	m_particleSampler = eg::Sampler(particleSamplerDesc);
	
	m_descriptorSet = eg::DescriptorSet(m_pipeline, 0);
	m_descriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	m_descriptorSet.BindTexture(eg::GetAsset<eg::Texture>("Textures/ForceFieldParticle.png"), 1, &m_particleSampler);
}

size_t ForceFieldMaterial::PipelineHash() const
{
	return typeid(ForceFieldMaterial).hash_code();
}

bool ForceFieldMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	MeshDrawArgs* mDrawArgs = static_cast<MeshDrawArgs*>(drawArgs);
	
	if (mDrawArgs->drawMode != MeshDrawMode::Emissive)
		return false;
	
	cmdCtx.BindPipeline(m_pipeline);
	
	cmdCtx.BindDescriptorSet(m_descriptorSet, 0);
	
	return true;
}

bool ForceFieldMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	return true;
}
