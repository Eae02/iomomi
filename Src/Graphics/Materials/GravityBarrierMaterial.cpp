#include "GravityBarrierMaterial.hpp"

#include "../../Editor/EditorGraphics.hpp"
#include "../GraphicsCommon.hpp"
#include "../RenderSettings.hpp"
#include "../SSR.hpp"
#include "MeshDrawArgs.hpp"

eg::Buffer GravityBarrierMaterial::s_sharedDataBuffer;

eg::Pipeline GravityBarrierMaterial::s_pipelineGameBeforeWater;
eg::Pipeline GravityBarrierMaterial::s_pipelineGameFinal;

eg::Pipeline GravityBarrierMaterial::s_pipelineEditor;

eg::Pipeline GravityBarrierMaterial::s_pipelineSSR;

static eg::DescriptorSetBinding descriptorSet0Bindings[] = {
	eg::DescriptorSetBinding{
		.binding = 0,
		.type = eg::BindingType::UniformBuffer,
		.shaderAccess = eg::ShaderAccessFlags::Vertex,
	},
	eg::DescriptorSetBinding{
		.binding = 1,
		.type = eg::BindingType::Texture,
		.shaderAccess = eg::ShaderAccessFlags::Fragment,
	},
	eg::DescriptorSetBinding{
		.binding = 2,
		.type = eg::BindingType::UniformBuffer,
		.shaderAccess = eg::ShaderAccessFlags::Fragment,
	},
};

static eg::DescriptorSet gravityBarrierSet0;

void GravityBarrierMaterial::OnInit()
{
	eg::SpecializationConstantEntry waterModeSpecConstant;
	waterModeSpecConstant.constantID = WATER_MODE_CONST_ID;
	waterModeSpecConstant.size = sizeof(int32_t);
	waterModeSpecConstant.offset = 0;

	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader =
		eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityBarrier/GravityBarrier.vs.glsl").DefaultVariant();
	pipelineCI.fragmentShader =
		eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityBarrier/GBGame.fs.glsl").DefaultVariant();
	pipelineCI.vertexBindings[0] = { 2, eg::InputRate::Vertex };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::UInt8Norm, 2, 0 };
	pipelineCI.topology = eg::Topology::TriangleStrip;
	pipelineCI.cullMode = eg::CullMode::None;
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	pipelineCI.enableDepthTest = true;
	pipelineCI.enableDepthWrite = false;
	pipelineCI.depthCompare = eg::CompareOp::Less;
	pipelineCI.blendStates[0] =
		eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::OneMinusSrcAlpha);
	pipelineCI.fragmentShader.specConstants = { &waterModeSpecConstant, 1 };
	pipelineCI.fragmentShader.specConstantsDataSize = sizeof(int32_t);
	pipelineCI.colorAttachmentFormats[0] = lightColorAttachmentFormat;
	pipelineCI.depthAttachmentFormat = GB_DEPTH_FORMAT;

	pipelineCI.label = "GravityBarrier[BeforeWater]";
	pipelineCI.fragmentShader.specConstantsData = const_cast<int32_t*>(&WATER_MODE_BEFORE);
	s_pipelineGameBeforeWater = eg::Pipeline::Create(pipelineCI);

	pipelineCI.label = "GravityBarrier[Final]";
	pipelineCI.fragmentShader.specConstantsData = const_cast<int32_t*>(&WATER_MODE_AFTER);
	s_pipelineGameFinal = eg::Pipeline::Create(pipelineCI);

	pipelineCI.label = "GravityBarrier[Editor]";
	pipelineCI.fragmentShader =
		eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityBarrier/GBEditor.fs.glsl").DefaultVariant();
	pipelineCI.colorAttachmentFormats[0] = EDITOR_COLOR_FORMAT;
	pipelineCI.depthAttachmentFormat = EDITOR_DEPTH_FORMAT;
	s_pipelineEditor = eg::Pipeline::Create(pipelineCI);

	eg::SpecializationConstantEntry ssrDistSpecConstant;
	ssrDistSpecConstant.constantID = 0;
	ssrDistSpecConstant.size = sizeof(float);
	ssrDistSpecConstant.offset = 0;

	eg::GraphicsPipelineCreateInfo ssrPipelineCI;
	ssrPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").DefaultVariant();
	ssrPipelineCI.fragmentShader =
		eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityBarrier/GBSSR.fs.glsl").DefaultVariant();
	ssrPipelineCI.enableDepthTest = true;
	ssrPipelineCI.enableDepthWrite = true;
	ssrPipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	ssrPipelineCI.depthCompare = eg::CompareOp::Less;
	ssrPipelineCI.label = "GravityBarrier[SSR]";
	ssrPipelineCI.fragmentShader.specConstants = { &ssrDistSpecConstant, 1 };
	ssrPipelineCI.fragmentShader.specConstantsDataSize = sizeof(float);
	ssrPipelineCI.fragmentShader.specConstantsData = const_cast<float*>(&SSR::MAX_DISTANCE);
	ssrPipelineCI.colorAttachmentFormats[0] = GetFormatForRenderTexture(RenderTex::SSRTemp1, true);
	ssrPipelineCI.depthAttachmentFormat = GetFormatForRenderTexture(RenderTex::SSRDepth, true);
	s_pipelineSSR = eg::Pipeline::Create(ssrPipelineCI);

	s_sharedDataBuffer =
		eg::Buffer(eg::BufferFlags::Update | eg::BufferFlags::UniformBuffer, sizeof(BarrierBufferData), nullptr);

	gravityBarrierSet0 = eg::DescriptorSet(descriptorSet0Bindings);
	gravityBarrierSet0.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	gravityBarrierSet0.BindTexture(eg::GetAsset<eg::Texture>("Textures/LineNoise.png"), 1, &linearRepeatSampler);
	gravityBarrierSet0.BindUniformBuffer(s_sharedDataBuffer, 2, 0, sizeof(BarrierBufferData));
}

void GravityBarrierMaterial::OnShutdown()
{
	s_pipelineGameBeforeWater.Destroy();
	s_pipelineGameFinal.Destroy();
	s_pipelineEditor.Destroy();
	s_pipelineSSR.Destroy();
	s_sharedDataBuffer.Destroy();
	gravityBarrierSet0.Destroy();
}

EG_ON_INIT(GravityBarrierMaterial::OnInit)
EG_ON_SHUTDOWN(GravityBarrierMaterial::OnShutdown)

void GravityBarrierMaterial::UpdateSharedDataBuffer(const BarrierBufferData& data)
{
	s_sharedDataBuffer.DCUpdateData(0, sizeof(GravityBarrierMaterial::BarrierBufferData), &data);
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
		cmdCtx.BindDescriptorSet(gravityBarrierSet0, 0);
		cmdCtx.BindTexture(mDrawArgs->waterDepthTexture, 1, 0, &framebufferNearestSampler);
		cmdCtx.BindTexture(blackPixelTexture, 1, 1, &framebufferNearestSampler);
		return true;
	}
	if (mDrawArgs->drawMode == MeshDrawMode::TransparentBeforeBlur)
	{
		cmdCtx.BindPipeline(s_pipelineGameFinal);
		cmdCtx.BindDescriptorSet(gravityBarrierSet0, 0);
		cmdCtx.BindTexture(mDrawArgs->waterDepthTexture, 1, 0, &framebufferNearestSampler);
		cmdCtx.BindTexture(
			mDrawArgs->rtManager->GetRenderTexture(RenderTex::BlurredGlassDepth), 1, 1, &framebufferNearestSampler);
		return true;
	}
	else if (mDrawArgs->drawMode == MeshDrawMode::TransparentFinal)
	{
		cmdCtx.BindPipeline(s_pipelineGameFinal);
		cmdCtx.BindDescriptorSet(gravityBarrierSet0, 0);
		cmdCtx.BindTexture(mDrawArgs->waterDepthTexture, 1, 0, &framebufferNearestSampler);
		cmdCtx.BindTexture(blackPixelTexture, 1, 1, &framebufferNearestSampler);
		return true;
	}
	else if (mDrawArgs->drawMode == MeshDrawMode::Editor)
	{
		cmdCtx.BindPipeline(s_pipelineEditor);
		cmdCtx.BindDescriptorSet(gravityBarrierSet0, 0);
		return true;
	}
	else if (mDrawArgs->drawMode == MeshDrawMode::AdditionalSSR)
	{
		cmdCtx.BindPipeline(s_pipelineSSR);
		cmdCtx.BindDescriptorSet(gravityBarrierSet0, 0);
		cmdCtx.BindTexture(
			mDrawArgs->rtManager->GetRenderTexture(RenderTex::GBColor2), 1, 0, &framebufferNearestSampler);
		cmdCtx.BindTexture(
			mDrawArgs->rtManager->GetRenderTexture(RenderTex::GBDepth), 1, 1, &framebufferNearestSampler);
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
	eg::DC.PushConstants(
		0, drawMode == MeshDrawMode::Editor ? offsetof(PushConstants, blockedAxis) : sizeof(PushConstants), &pc);

	if (drawMode != MeshDrawMode::Editor)
	{
		eg::TextureRef tex = waterDistanceTexture.handle ? waterDistanceTexture : eg::TextureRef(whitePixelTexture);
		eg::DC.BindTexture(tex, 1, 2, &framebufferLinearSampler);
	}

	return true;
}

eg::IMaterial::VertexInputConfiguration GravityBarrierMaterial::GetVertexInputConfiguration(const void* drawArgs) const
{
	return VertexInputConfiguration{ .vertexBindingsMask = 1 };
}
