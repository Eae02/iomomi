#include "GravityBarrierMaterial.hpp"
#include "MeshDrawArgs.hpp"
#include "../SSR.hpp"
#include "../GraphicsCommon.hpp"
#include "../RenderSettings.hpp"

eg::Buffer GravityBarrierMaterial::s_sharedDataBuffer;

eg::Pipeline GravityBarrierMaterial::s_pipelineGameBeforeWater;
eg::Pipeline GravityBarrierMaterial::s_pipelineGameFinal;

eg::Pipeline GravityBarrierMaterial::s_pipelineEditor;

eg::Pipeline GravityBarrierMaterial::s_pipelineSSR;

void GravityBarrierMaterial::OnInit()
{
	eg::SpecializationConstantEntry waterModeSpecConstant;
	waterModeSpecConstant.constantID = WATER_MODE_CONST_ID;
	waterModeSpecConstant.size = sizeof(int32_t);
	waterModeSpecConstant.offset = 0;
	
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityBarrier/GravityBarrier.vs.glsl").DefaultVariant();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityBarrier/GBGame.fs.glsl").DefaultVariant();
	pipelineCI.vertexBindings[0] = { 2, eg::InputRate::Vertex };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::UInt8Norm, 2, 0 };
	pipelineCI.topology = eg::Topology::TriangleStrip;
	pipelineCI.cullMode = eg::CullMode::None;
	pipelineCI.enableDepthTest = true;
	pipelineCI.enableDepthWrite = false;
	pipelineCI.depthCompare = eg::CompareOp::Less;
	pipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::OneMinusSrcAlpha);
	pipelineCI.fragmentShader.specConstants = { &waterModeSpecConstant, 1 };
	pipelineCI.fragmentShader.specConstantsDataSize = sizeof(int32_t);
	
	pipelineCI.label = "GravityBarrier[BeforeWater]";
	pipelineCI.fragmentShader.specConstantsData = const_cast<int32_t*>(&WATER_MODE_BEFORE);
	s_pipelineGameBeforeWater = eg::Pipeline::Create(pipelineCI);
	
	pipelineCI.label = "GravityBarrier[Final]";
	pipelineCI.fragmentShader.specConstantsData = const_cast<int32_t*>(&WATER_MODE_AFTER);
	s_pipelineGameFinal = eg::Pipeline::Create(pipelineCI);
	
	pipelineCI.label = "GravityBarrier[Editor]";
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityBarrier/GBEditor.fs.glsl").DefaultVariant();
	s_pipelineEditor = eg::Pipeline::Create(pipelineCI);
	
	eg::SpecializationConstantEntry ssrDistSpecConstant;
	ssrDistSpecConstant.constantID = 0;
	ssrDistSpecConstant.size = sizeof(float);
	ssrDistSpecConstant.offset = 0;
	
	eg::GraphicsPipelineCreateInfo ssrPipelineCI;
	ssrPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").DefaultVariant();
	ssrPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityBarrier/GBSSR.fs.glsl").DefaultVariant();
	ssrPipelineCI.enableDepthTest = true;
	ssrPipelineCI.enableDepthWrite = true;
	ssrPipelineCI.depthCompare = eg::CompareOp::Less;
	ssrPipelineCI.label = "GravityBarrier[SSR]";
	ssrPipelineCI.fragmentShader.specConstants = { &ssrDistSpecConstant, 1 };
	ssrPipelineCI.fragmentShader.specConstantsDataSize = sizeof(float);
	ssrPipelineCI.fragmentShader.specConstantsData = const_cast<float*>(&SSR::MAX_DISTANCE);
	s_pipelineSSR = eg::Pipeline::Create(ssrPipelineCI);
	
	s_sharedDataBuffer = eg::Buffer(eg::BufferFlags::Update | eg::BufferFlags::UniformBuffer, sizeof(BarrierBufferData), nullptr);
}

void GravityBarrierMaterial::OnShutdown()
{
	s_pipelineGameBeforeWater.Destroy();
	s_pipelineGameFinal.Destroy();
	s_pipelineEditor.Destroy();
	s_pipelineSSR.Destroy();
	s_sharedDataBuffer.Destroy();
}

EG_ON_INIT(GravityBarrierMaterial::OnInit)
EG_ON_SHUTDOWN(GravityBarrierMaterial::OnShutdown)

void GravityBarrierMaterial::UpdateSharedDataBuffer(const BarrierBufferData& data)
{
	eg::DC.UpdateBuffer(s_sharedDataBuffer, 0, sizeof(GravityBarrierMaterial::BarrierBufferData), &data);
	s_sharedDataBuffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Fragment);
}

size_t GravityBarrierMaterial::PipelineHash() const
{
	return typeid(GravityBarrierMaterial).hash_code();
}

bool GravityBarrierMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	MeshDrawArgs* mDrawArgs = reinterpret_cast<MeshDrawArgs*>(drawArgs);
	
	if (mDrawArgs->drawMode == MeshDrawMode::TransparentBeforeWater)
	{
		cmdCtx.BindPipeline(s_pipelineGameBeforeWater);
		cmdCtx.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
		cmdCtx.BindTexture(eg::GetAsset<eg::Texture>("Textures/LineNoise.png"), 0, 1);
		cmdCtx.BindUniformBuffer(s_sharedDataBuffer, 0, 2, 0, sizeof(BarrierBufferData));
		cmdCtx.BindTexture(mDrawArgs->waterDepthTexture, 0, 3);
		cmdCtx.BindTexture(blackPixelTexture, 0, 4);
		return true;
	}
	if (mDrawArgs->drawMode == MeshDrawMode::TransparentBeforeBlur)
	{
		cmdCtx.BindPipeline(s_pipelineGameFinal);
		cmdCtx.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
		cmdCtx.BindTexture(eg::GetAsset<eg::Texture>("Textures/LineNoise.png"), 0, 1);
		cmdCtx.BindUniformBuffer(s_sharedDataBuffer, 0, 2, 0, sizeof(BarrierBufferData));
		cmdCtx.BindTexture(mDrawArgs->waterDepthTexture, 0, 3);
		cmdCtx.BindTexture(mDrawArgs->rtManager->GetRenderTexture(RenderTex::BlurredGlassDepth), 0, 4);
		return true;
	}
	else if (mDrawArgs->drawMode == MeshDrawMode::TransparentFinal)
	{
		cmdCtx.BindPipeline(s_pipelineGameFinal);
		cmdCtx.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
		cmdCtx.BindTexture(eg::GetAsset<eg::Texture>("Textures/LineNoise.png"), 0, 1);
		cmdCtx.BindUniformBuffer(s_sharedDataBuffer, 0, 2, 0, sizeof(BarrierBufferData));
		cmdCtx.BindTexture(mDrawArgs->waterDepthTexture, 0, 3);
		cmdCtx.BindTexture(blackPixelTexture, 0, 4);
		return true;
	}
	else if (mDrawArgs->drawMode == MeshDrawMode::Editor)
	{
		cmdCtx.BindPipeline(s_pipelineEditor);
		cmdCtx.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
		cmdCtx.BindTexture(eg::GetAsset<eg::Texture>("Textures/LineNoise.png"), 0, 1);
		return true;
	}
	else if (mDrawArgs->drawMode == MeshDrawMode::AdditionalSSR)
	{
		cmdCtx.BindPipeline(s_pipelineSSR);
		cmdCtx.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
		cmdCtx.BindTexture(eg::GetAsset<eg::Texture>("Textures/LineNoise.png"), 0, 1);
		cmdCtx.BindUniformBuffer(s_sharedDataBuffer, 0, 2, 0, sizeof(BarrierBufferData));
		cmdCtx.BindTexture(mDrawArgs->rtManager->GetRenderTexture(RenderTex::GBColor2), 0, 3);
		cmdCtx.BindTexture(mDrawArgs->rtManager->GetRenderTexture(RenderTex::GBDepth), 0, 4);
		return true;
	}
	
	return false;
}

static float* opacityScale = eg::TweakVarFloat("gb_opacity_scale", 1.6f, 0.0f);
static float* lineWidth = eg::TweakVarFloat("gb_line_width", 0.001f, 0.0f);
static float* intensityDecay = eg::TweakVarFloat("gb_glow_decay", 15.0f, 0.0f);
static float* glowIntensity = eg::TweakVarFloat("gb_glow_intensity", 0.1f, 0.0f);

bool GravityBarrierMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	MeshDrawMode drawMode = reinterpret_cast<MeshDrawArgs*>(drawArgs)->drawMode;
	
	struct PushConstants
	{
		glm::vec3 position;
		float opacity;
		glm::vec4 tangent;
		glm::vec4 bitangent;
		uint32_t blockedAxis;
		float noiseSampleOffset;
		float lineWidth;
		float lineGlowDecay;
		float lineGlowIntensity;
	};
	
	PushConstants pc;
	pc.position = position;
	pc.opacity = opacity * *opacityScale;
	pc.tangent = glm::vec4(glm::normalize(tangent), glm::length(tangent));
	pc.bitangent = glm::vec4(glm::normalize(bitangent), glm::length(bitangent));
	pc.blockedAxis = blockedAxis;
	pc.noiseSampleOffset = noiseSampleOffset;
	pc.lineWidth = *lineWidth;
	pc.lineGlowDecay = -*intensityDecay;
	pc.lineGlowIntensity = *glowIntensity;
	eg::DC.PushConstants(0, drawMode == MeshDrawMode::Editor ? offsetof(PushConstants, blockedAxis) : sizeof(PushConstants), &pc);
	
	if (drawMode != MeshDrawMode::Editor)
	{
		eg::TextureRef tex = waterDistanceTexture.handle ? waterDistanceTexture : eg::TextureRef(whitePixelTexture);
		eg::DC.BindTexture(tex, 0, 5, &framebufferLinearSampler);
	}
	
	return true;
}
