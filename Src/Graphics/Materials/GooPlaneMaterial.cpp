#include "GooPlaneMaterial.hpp"
#include "../../World/Entities/Components/LiquidPlaneComp.hpp"

constexpr int NM_SAMPLES = 3;
constexpr float NM_SCALE_GLOBAL = 3.5f;
constexpr float NM_SPEED_GLOBAL = 0.15f;
constexpr float NM_ANGLES[NM_SAMPLES] = { 0.25f * eg::TWO_PI, 0.5f * eg::TWO_PI, 0.75f * eg::TWO_PI };
constexpr float NM_SCALES[NM_SAMPLES] = { 1.2f, 1.0f, 0.8f };
constexpr float NM_SPEED[NM_SAMPLES] = { 0.75f, 1.0f, 1.25f };

eg::Pipeline GooPlaneMaterial::s_pipeline;
eg::Buffer GooPlaneMaterial::s_textureTransformsBuffer;
eg::DescriptorSet GooPlaneMaterial::s_descriptorSet;

static glm::vec3 waterColor;

void GooPlaneMaterial::OnInit()
{
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GooPlane.vs.glsl").DefaultVariant();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GooPlane.fs.glsl").DefaultVariant();
	pipelineCI.enableDepthWrite = true;
	pipelineCI.enableDepthTest = true;
	pipelineCI.cullMode = eg::CullMode::Back;
	pipelineCI.cullMode = eg::CullMode::Back;
	pipelineCI.frontFaceCCW = true;
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	pipelineCI.vertexBindings[0] = { sizeof(LiquidPlaneComp::Vertex), eg::InputRate::Vertex };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, offsetof(LiquidPlaneComp::Vertex, pos) };
	pipelineCI.vertexAttributes[1] = { 0, eg::DataType::UInt8Norm, 4, offsetof(LiquidPlaneComp::Vertex, edgeDists) };
	pipelineCI.blendStates[0] = eg::AlphaBlend;
	s_pipeline = eg::Pipeline::Create(pipelineCI);
	
	s_descriptorSet = eg::DescriptorSet(s_pipeline, 0);
	
	struct
	{
		glm::vec4 color;
		glm::vec4 nmTransforms[NM_SAMPLES * 2];
	} bufferData;
	
	waterColor = glm::vec3(0.1f, 0.9f, 0.2f);
	bufferData.color = glm::vec4(waterColor, 0.3f);
	
	for (int i = 0; i < NM_SAMPLES; i++)
	{
		const float sinT = std::sin(NM_ANGLES[i]);
		const float cosT = std::cos(NM_ANGLES[i]);
		const float scale = 1.0f / (NM_SCALES[i] * NM_SCALE_GLOBAL);
		bufferData.nmTransforms[i * 2 + 0] = glm::vec4(cosT, 0, -sinT, NM_SPEED[i] * NM_SPEED_GLOBAL) * scale;
		bufferData.nmTransforms[i * 2 + 1] = glm::vec4(sinT, 0,  cosT, NM_SPEED[i] * NM_SPEED_GLOBAL) * scale;
	}
	s_textureTransformsBuffer = eg::Buffer(eg::BufferFlags::UniformBuffer, sizeof(bufferData), &bufferData);
	s_textureTransformsBuffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Fragment);
	
	s_descriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	s_descriptorSet.BindUniformBuffer(s_textureTransformsBuffer, 1, 0, sizeof(bufferData));
	s_descriptorSet.BindTexture(eg::GetAsset<eg::Texture>("Textures/SlimeN.png"), 2);
}

void GooPlaneMaterial::OnShutdown()
{
	GooPlaneMaterial::s_pipeline = { };
	GooPlaneMaterial::s_descriptorSet = { };
	GooPlaneMaterial::s_textureTransformsBuffer = { };
}

EG_ON_INIT(GooPlaneMaterial::OnInit)
EG_ON_SHUTDOWN(GooPlaneMaterial::OnShutdown)

size_t GooPlaneMaterial::PipelineHash() const
{
	return typeid(GooPlaneMaterial).hash_code();
}

bool GooPlaneMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	MeshDrawArgs& meshDrawArgs = *static_cast<MeshDrawArgs*>(drawArgs);
	if (meshDrawArgs.drawMode != MeshDrawMode::Emissive)
		return false;
	
	cmdCtx.BindPipeline(s_pipeline);
	
	cmdCtx.BindDescriptorSet(s_descriptorSet, 0);
	
	return true;
}

bool GooPlaneMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	return true;
}
