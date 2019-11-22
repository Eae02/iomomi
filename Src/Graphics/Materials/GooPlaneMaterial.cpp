#include "GooPlaneMaterial.hpp"

constexpr int NM_SAMPLES = 3;
constexpr float NM_SCALE_GLOBAL = 3.5f;
constexpr float NM_SPEED_GLOBAL = 0.15f;
constexpr float NM_ANGLES[NM_SAMPLES] = { 0.25f * eg::TWO_PI, 0.5f * eg::TWO_PI, 0.75f * eg::TWO_PI };
constexpr float NM_SCALES[NM_SAMPLES] = { 1.2f, 1.0f, 0.8f };
constexpr float NM_SPEED[NM_SAMPLES] = { 0.75f, 1.0f, 1.25f };

eg::Pipeline GooPlaneMaterial::s_pipelineReflEnabled;
eg::Pipeline GooPlaneMaterial::s_pipelineReflDisabled;
eg::Pipeline GooPlaneMaterial::s_emissivePipeline;
eg::Buffer GooPlaneMaterial::s_textureTransformsBuffer;
eg::DescriptorSet GooPlaneMaterial::s_descriptorSet;

static glm::vec3 waterColor;

static const eg::StencilState stencilState = DeferredRenderer::MakeStencilState(0);

void GooPlaneMaterial::OnInit()
{
	eg::ShaderModuleAsset& fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GooPlane.fs.glsl");
	
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GooPlane.vs.glsl").DefaultVariant();
	pipelineCI.fragmentShader = fragmentShader.GetVariant("VDefault");
	pipelineCI.enableDepthWrite = true;
	pipelineCI.enableDepthTest = true;
	pipelineCI.cullMode = eg::CullMode::Back;
	pipelineCI.frontFaceCCW = true;
	pipelineCI.numColorAttachments = 2;
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	pipelineCI.setBindModes[1] = eg::BindMode::Dynamic;
	pipelineCI.vertexBindings[0] = { sizeof(glm::vec3), eg::InputRate::Vertex };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, 0 };
	pipelineCI.enableStencilTest = true;
	pipelineCI.frontStencilState = stencilState;
	pipelineCI.backStencilState = stencilState;
	
	s_pipelineReflEnabled = eg::Pipeline::Create(pipelineCI);
	
	pipelineCI.fragmentShader = fragmentShader.GetVariant("VNoRefl");
	s_pipelineReflDisabled = eg::Pipeline::Create(pipelineCI);
	
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GooPlane-Emissive.fs.glsl").DefaultVariant();
	pipelineCI.enableStencilTest = false;
	pipelineCI.numColorAttachments = 1;
	pipelineCI.depthCompare = eg::CompareOp::LessOrEqual;
	pipelineCI.enableDepthWrite = false;
	s_emissivePipeline = eg::Pipeline::Create(pipelineCI);
	
	s_descriptorSet = eg::DescriptorSet(s_pipelineReflEnabled, 0);
	
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
	GooPlaneMaterial::s_pipelineReflEnabled = { };
	GooPlaneMaterial::s_pipelineReflDisabled = { };
	GooPlaneMaterial::s_emissivePipeline = { };
	GooPlaneMaterial::s_descriptorSet = { };
	GooPlaneMaterial::s_textureTransformsBuffer = { };
}

EG_ON_INIT(GooPlaneMaterial::OnInit)
EG_ON_SHUTDOWN(GooPlaneMaterial::OnShutdown)

size_t GooPlaneMaterial::PipelineHash() const
{
	size_t h = typeid(GooPlaneMaterial).hash_code();
	if (m_reflectionPlane.texture.handle == nullptr)
		h++;
	return h;
}

bool GooPlaneMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	MeshDrawMode drawMode = static_cast<MeshDrawArgs*>(drawArgs)->drawMode;
	if (!(drawMode == MeshDrawMode::Game || drawMode == MeshDrawMode::Emissive))
		return false;
	
	if (drawMode == MeshDrawMode::Emissive)
		cmdCtx.BindPipeline(s_emissivePipeline);
	else if (m_reflectionPlane.texture.handle)
		cmdCtx.BindPipeline(s_pipelineReflEnabled);
	else
		cmdCtx.BindPipeline(s_pipelineReflDisabled);
	
	cmdCtx.BindDescriptorSet(s_descriptorSet, 0);
	
	return true;
}

bool GooPlaneMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	if (static_cast<MeshDrawArgs*>(drawArgs)->drawMode == MeshDrawMode::Game &&
		m_reflectionPlane.texture.handle != nullptr)
	{
		cmdCtx.BindTexture(m_reflectionPlane.texture, 1, 0);
	}
	
	return true;
}
