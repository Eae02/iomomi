#include "GravityBarrierMaterial.hpp"

#include "../../Editor/EditorGraphics.hpp"
#include "../../Water/WaterBarrierRenderer.hpp"
#include "../GraphicsCommon.hpp"
#include "../RenderSettings.hpp"
#include "../RenderTargets.hpp"
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
		.type = eg::BindingTypeUniformBuffer{},
		.shaderAccess = eg::ShaderAccessFlags::Vertex | eg::ShaderAccessFlags::Fragment,
	},
	eg::DescriptorSetBinding{
		.binding = 1,
		.type = eg::BindingTypeTexture{},
		.shaderAccess = eg::ShaderAccessFlags::Fragment,
	},
	eg::DescriptorSetBinding{
		.binding = 2,
		.type = eg::BindingTypeSampler::Default,
		.shaderAccess = eg::ShaderAccessFlags::Fragment,
	},
	eg::DescriptorSetBinding{
		.binding = 3,
		.type = eg::BindingTypeUniformBuffer{},
		.shaderAccess = eg::ShaderAccessFlags::Fragment,
	},
};

static std::array<eg::DescriptorSetBinding, 3> descriptorSet1Bindings;
static bool canLinearFilterWaterDepthTexture;

static eg::DescriptorSet gravityBarrierSet0;

void GravityBarrierMaterial::OnInit()
{
	canLinearFilterWaterDepthTexture = eg::HasFlag(
		eg::gal::GetFormatCapabilities(WaterBarrierRenderer::DEPTH_TEXTURE_FORMAT),
		eg::FormatCapabilities::SampledImageFilterLinear
	);

	descriptorSet1Bindings = {
		eg::DescriptorSetBinding{
			.binding = 0,
			.type = eg::BindingTypeUniformBuffer{},
			.shaderAccess = eg::ShaderAccessFlags::Vertex | eg::ShaderAccessFlags::Fragment,
		},
		eg::DescriptorSetBinding{
			.binding = 1,
			.type =
				eg::BindingTypeTexture{
					.sampleMode = canLinearFilterWaterDepthTexture ? eg::TextureSampleMode::Float
		                                                           : eg::TextureSampleMode::UnfilterableFloat,
				},
			.shaderAccess = eg::ShaderAccessFlags::Fragment,
		},
		eg::DescriptorSetBinding{
			.binding = 2,
			.type =
				canLinearFilterWaterDepthTexture ? eg::BindingTypeSampler::Default : eg::BindingTypeSampler::Nearest,
			.shaderAccess = eg::ShaderAccessFlags::Fragment,
		},
	};

	eg::SpecializationConstantEntry specConstants[1];
	specConstants[0].constantID = WATER_MODE_CONST_ID;

	const auto& vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityBarrier/GravityBarrier.vs.glsl");

	const eg::BlendState blendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::OneMinusSrcAlpha);

	eg::GraphicsPipelineCreateInfo gamePipelineCI = {
		.vertexShader = vertexShader.ToStageInfo(),
		.fragmentShader = eg::ShaderStageInfo {
			.shaderModule = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityBarrier/GBGame.fs.glsl").DefaultVariant(),
			.specConstants = specConstants,
		},
		.enableDepthTest = true,
		.enableDepthWrite = false,
		.depthCompare = eg::CompareOp::Less,
		.topology = eg::Topology::TriangleStrip,
		.cullMode = eg::CullMode::None,
		.descriptorSetBindings = {
			descriptorSet0Bindings,
			descriptorSet1Bindings,
			{ &FRAGMENT_SHADER_TEXTURE_BINDING, 1 },
			{ &FRAGMENT_SHADER_TEXTURE_BINDING_UNFILTERABLE, 1 },
		},
		.colorAttachmentFormats ={ lightColorAttachmentFormat},
		.blendStates = { blendState },
		.depthAttachmentFormat = GB_DEPTH_FORMAT,
		.depthStencilUsage = eg::TextureUsage::DepthStencilReadOnly,
		.vertexBindings = { { sizeof(float) * 2, eg::InputRate::Vertex } },
		.vertexAttributes = { { 0, eg::DataType::Float32, 2, 0 } },
	};

	gamePipelineCI.label = "GravityBarrier[BeforeWater]";
	specConstants[0].value = WATER_MODE_BEFORE;
	s_pipelineGameBeforeWater = eg::Pipeline::Create(gamePipelineCI);

	gamePipelineCI.label = "GravityBarrier[Final]";
	specConstants[0].value = WATER_MODE_AFTER;
	s_pipelineGameFinal = eg::Pipeline::Create(gamePipelineCI);

	s_pipelineEditor = eg::Pipeline::Create(eg::GraphicsPipelineCreateInfo{
		.vertexShader = vertexShader.ToStageInfo(),
		.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityBarrier/GBEditor.fs.glsl").ToStageInfo(),
		.enableDepthTest = true,
		.enableDepthWrite = false,
		.depthCompare = eg::CompareOp::Less,
		.topology = eg::Topology::TriangleStrip,
		.cullMode = eg::CullMode::None,
		.descriptorSetBindings = { descriptorSet0Bindings, descriptorSet1Bindings },
		.colorAttachmentFormats = { EDITOR_COLOR_FORMAT },
		.blendStates = { blendState },
		.depthAttachmentFormat = EDITOR_DEPTH_FORMAT,
		.depthStencilUsage = eg::TextureUsage::FramebufferAttachment,
		.vertexBindings = { { sizeof(float) * 2, eg::InputRate::Vertex } },
		.vertexAttributes = { { 0, eg::DataType::Float32, 2, 0 } },
		.label = "GravityBarrier[Editor]",
	});

	const eg::SpecializationConstantEntry ssrDistSpecConstant = { 0, SSR::MAX_DISTANCE };
	s_pipelineSSR = eg::Pipeline::Create(eg::GraphicsPipelineCreateInfo {
		.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").ToStageInfo(),
		.fragmentShader = {
			.shaderModule = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityBarrier/GBSSR.fs.glsl").DefaultVariant(),
			.specConstants = { &ssrDistSpecConstant, 1 },
		},
		.enableDepthTest = true,
		.enableDepthWrite = true,
		.depthCompare = eg::CompareOp::Less,
		.descriptorSetBindings = { descriptorSet0Bindings, descriptorSet1Bindings },
		.colorAttachmentFormats = {SSR::COLOR_FORMAT},
		.depthAttachmentFormat = SSR::DEPTH_FORMAT,
		.label = "GravityBarrier[SSR]",
	});

	s_sharedDataBuffer =
		eg::Buffer(eg::BufferFlags::Update | eg::BufferFlags::UniformBuffer, sizeof(BarrierBufferData), nullptr);

	gravityBarrierSet0 = eg::DescriptorSet(descriptorSet0Bindings);
	gravityBarrierSet0.BindUniformBuffer(RenderSettings::instance->Buffer(), 0);
	gravityBarrierSet0.BindTexture(eg::GetAsset<eg::Texture>("Textures/LineNoise.png"), 1);
	gravityBarrierSet0.BindSampler(samplers::linearRepeat, 2);
	gravityBarrierSet0.BindUniformBuffer(s_sharedDataBuffer, 3);
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
		cmdCtx.BindDescriptorSet(mDrawArgs->renderTargets->GetWaterDepthTextureDescriptorSetOrDummy(), 2);
		cmdCtx.BindDescriptorSet(blackPixelTextureDescriptorSet, 3);
		return true;
	}
	if (mDrawArgs->drawMode == MeshDrawMode::TransparentBeforeBlur)
	{
		cmdCtx.BindPipeline(s_pipelineGameFinal);
		cmdCtx.BindDescriptorSet(gravityBarrierSet0, 0);
		cmdCtx.BindDescriptorSet(mDrawArgs->renderTargets->GetWaterDepthTextureDescriptorSetOrDummy(), 2);
		cmdCtx.BindDescriptorSet(mDrawArgs->renderTargets->blurredGlassDepthTextureDescriptorSet.value(), 3);
		return true;
	}
	else if (mDrawArgs->drawMode == MeshDrawMode::TransparentFinal)
	{
		cmdCtx.BindPipeline(s_pipelineGameFinal);
		cmdCtx.BindDescriptorSet(gravityBarrierSet0, 0);
		cmdCtx.BindDescriptorSet(mDrawArgs->renderTargets->GetWaterDepthTextureDescriptorSetOrDummy(), 2);
		cmdCtx.BindDescriptorSet(blackPixelTextureDescriptorSet, 3);
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
		cmdCtx.BindDescriptorSet(mDrawArgs->renderTargets->ssrAdditionalDescriptorSet.value(), 2);
		return true;
	}

	return false;
}

static float* opacityScale = eg::TweakVarFloat("gb_opacity_scale", 1.6f, 0.0f);
static float* lineWidth = eg::TweakVarFloat("gb_line_width", 0.001f, 0.0f);
static float* intensityDecay = eg::TweakVarFloat("gb_glow_decay", 15.0f, 0.0f);
static float* glowIntensity = eg::TweakVarFloat("gb_glow_intensity", 0.1f, 0.0f);

struct ParametersData
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

bool GravityBarrierMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	if (!m_hasSetWaterDistanceTexture)
	{
		m_hasSetWaterDistanceTexture = true;
		m_descriptorSet.BindTexture(eg::Texture::WhitePixel(), 1);
	}

	eg::DC.BindDescriptorSet(m_descriptorSet, 1);

	return true;
}

eg::IMaterial::VertexInputConfiguration GravityBarrierMaterial::GetVertexInputConfiguration(const void* drawArgs) const
{
	return VertexInputConfiguration{ .vertexBindingsMask = 1 };
}

void GravityBarrierMaterial::SetParameters(const Parameters& parameters)
{
	if (parameters == m_currentAssignedParameters)
		return;

	m_currentAssignedParameters = parameters;

	ParametersData data = {
		.position = parameters.position,
		.opacity = parameters.opacity * *opacityScale,
		.tangent = glm::vec4(glm::normalize(parameters.tangent), glm::length(parameters.tangent)),
		.bitangent = glm::vec4(glm::normalize(parameters.bitangent), glm::length(parameters.bitangent)),
		.blockedAxis = parameters.blockedAxis,
		.noiseSampleOffset = parameters.noiseSampleOffset,
		.lineWidth = *lineWidth,
		.lineGlowDecay = -*intensityDecay,
		.lineGlowIntensity = *glowIntensity,
	};

	m_parametersBuffer.DCUpdateData<ParametersData>(0, { &data, 1 });
}

GravityBarrierMaterial::GravityBarrierMaterial()
{
	m_parametersBuffer =
		eg::Buffer(eg::BufferFlags::CopyDst | eg::BufferFlags::UniformBuffer, sizeof(ParametersData), nullptr);

	m_descriptorSet = eg::DescriptorSet(descriptorSet1Bindings);
	m_descriptorSet.BindUniformBuffer(m_parametersBuffer, 0);
	m_descriptorSet.BindSampler(canLinearFilterWaterDepthTexture ? samplers::linearClamp : samplers::nearestClamp, 2);
}

void GravityBarrierMaterial::SetWaterDistanceTexture(eg::TextureRef waterDistanceTexture)
{
	m_descriptorSet.BindTexture(waterDistanceTexture, 1);
	m_hasSetWaterDistanceTexture = true;
}
