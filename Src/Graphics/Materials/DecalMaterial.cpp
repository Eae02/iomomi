#include "DecalMaterial.hpp"

#include "../../Editor/EditorGraphics.hpp"
#include "../RenderSettings.hpp"
#include "../Vertex.hpp"
#include "MeshDrawArgs.hpp"

static eg::Pipeline decalsGamePipeline;
static eg::Pipeline decalsGamePipelineInheritNormals;
static eg::Pipeline decalsEditorPipeline;

static eg::Buffer decalVertexBuffer;

static eg::MeshBuffersDescriptor decalBuffersDescriptor;

static float decalVertexData[] = { -1, -1, 1, -1, -1, 1, 1, 1 };

static_assert(offsetof(DecalMaterial::InstanceData, transform) == 0);
static_assert(offsetof(DecalMaterial::InstanceData, textureMin) == sizeof(float) * 12);
static_assert(offsetof(DecalMaterial::InstanceData, textureMax) == sizeof(float) * 14);

void DecalMaterial::LazyInitGlobals()
{
	if (decalsGamePipeline.handle != nullptr)
		return;

	const eg::ShaderModuleAsset& vs = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Decal.vs.glsl");
	const eg::ShaderModuleAsset& fs = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Decal.fs.glsl");

	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = vs.GetVariant("VDefault");
	pipelineCI.fragmentShader = fs.GetVariant("VDefault");
	pipelineCI.enableDepthTest = true;
	pipelineCI.enableDepthWrite = false;
	pipelineCI.numColorAttachments = 2;
	pipelineCI.cullMode = eg::CullMode::Back;
	pipelineCI.topology = eg::Topology::TriangleStrip;
	pipelineCI.depthCompare = eg::CompareOp::LessOrEqual;
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	pipelineCI.vertexBindings[POSITION_BINDING] = { sizeof(float) * 2, eg::InputRate::Vertex };
	pipelineCI.vertexBindings[INSTANCE_DATA_BINDING] = { sizeof(DecalMaterial::InstanceData), eg::InputRate::Instance };
	pipelineCI.vertexAttributes[0] = { POSITION_BINDING, eg::DataType::Float32, 2, 0 };
	pipelineCI.vertexAttributes[1] = { INSTANCE_DATA_BINDING, eg::DataType::Float32, 4, 0 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[2] = { INSTANCE_DATA_BINDING, eg::DataType::Float32, 4, 1 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[3] = { INSTANCE_DATA_BINDING, eg::DataType::Float32, 4, 2 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[4] = { INSTANCE_DATA_BINDING, eg::DataType::Float32, 4, 3 * sizeof(float) * 4 };
	pipelineCI.blendStates[0] = eg::BlendState(
		eg::BlendFunc::Add, eg::BlendFunc::Add,
		/* srcColor */ eg::BlendFactor::SrcAlpha,
		/* srcAlpha */ eg::BlendFactor::ConstantAlpha,
		/* dstColor */ eg::BlendFactor::OneMinusSrcAlpha,
		/* dstAlpha */ eg::BlendFactor::OneMinusSrcAlpha);
	pipelineCI.blendStates[1] = eg::BlendState(
		eg::BlendFunc::Add, eg::BlendFunc::Add,
		/* srcColor */ eg::BlendFactor::SrcAlpha,
		/* srcAlpha */ eg::BlendFactor::Zero,
		/* dstColor */ eg::BlendFactor::OneMinusSrcAlpha,
		/* dstAlpha */ eg::BlendFactor::OneMinusSrcAlpha);
	pipelineCI.blendConstants[3] = 1.0f;
	pipelineCI.colorAttachmentFormats[0] = GB_COLOR_FORMAT;
	pipelineCI.colorAttachmentFormats[1] = GB_COLOR_FORMAT;
	pipelineCI.depthAttachmentFormat = GB_DEPTH_FORMAT;
	pipelineCI.label = "DecalsGame";
	decalsGamePipeline = eg::Pipeline::Create(pipelineCI);

	pipelineCI.blendStates[1].colorWriteMask = eg::ColorWriteMask::A | eg::ColorWriteMask::B;
	decalsGamePipelineInheritNormals = eg::Pipeline::Create(pipelineCI);

	pipelineCI.fragmentShader = fs.GetVariant("VEditor");
	pipelineCI.numColorAttachments = 1;
	pipelineCI.blendStates[1] = {};
	pipelineCI.colorAttachmentFormats[0] = EDITOR_COLOR_FORMAT;
	pipelineCI.depthAttachmentFormat = EDITOR_DEPTH_FORMAT;
	pipelineCI.label = "DecalsEditor";
	decalsEditorPipeline = eg::Pipeline::Create(pipelineCI);

	decalVertexBuffer = eg::Buffer(eg::BufferFlags::VertexBuffer, sizeof(decalVertexData), decalVertexData);
	decalVertexBuffer.UsageHint(eg::BufferUsage::VertexBuffer);

	decalBuffersDescriptor = {
		.vertexBuffer = decalVertexBuffer,
		.vertexStreamOffsets = { 0 },
	};
}

static void OnShutdown()
{
	decalsGamePipeline.Destroy();
	decalsGamePipelineInheritNormals.Destroy();
	decalsEditorPipeline.Destroy();
	decalVertexBuffer.Destroy();
}

EG_ON_SHUTDOWN(OnShutdown)

DecalMaterial::DecalMaterial(const eg::Texture& albedoTexture, const eg::Texture& normalMapTexture)
	: m_albedoTexture(albedoTexture), m_normalMapTexture(normalMapTexture),
	  m_aspectRatio(albedoTexture.Width() / (float)albedoTexture.Height()), m_descriptorSet(decalsGamePipeline, 0)
{
	m_descriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	m_descriptorSet.BindTexture(m_albedoTexture, 1, &commonTextureSampler);
	m_descriptorSet.BindTexture(m_normalMapTexture, 2, &commonTextureSampler);
}

size_t DecalMaterial::PipelineHash() const
{
	return typeid(DecalMaterial).hash_code() + inheritNormals;
}

bool DecalMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	MeshDrawArgs* mDrawArgs = reinterpret_cast<MeshDrawArgs*>(drawArgs);

	if (mDrawArgs->drawMode == MeshDrawMode::Game)
	{
		cmdCtx.BindPipeline(inheritNormals ? decalsGamePipelineInheritNormals : decalsGamePipeline);
	}
	else if (mDrawArgs->drawMode == MeshDrawMode::Editor)
	{
		cmdCtx.BindPipeline(decalsEditorPipeline);
	}
	else
	{
		return false;
	}

	return true;
}

bool DecalMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	float pc[2] = { roughness, opacity };
	cmdCtx.PushConstants(0, sizeof(pc), &pc);

	cmdCtx.BindDescriptorSet(m_descriptorSet, 0);

	return true;
}

eg::MeshBatch::Mesh DecalMaterial::GetMesh()
{
	return {
		.buffersDescriptor = &decalBuffersDescriptor,
		.numElements = 4,
	};
}

bool DecalMaterial::CheckInstanceDataType(const std::type_info* instanceDataType) const
{
	return instanceDataType == &typeid(InstanceData);
}

eg::IMaterial::VertexInputConfiguration DecalMaterial::GetVertexInputConfiguration(const void* drawArgs) const
{
	return VertexInputConfiguration{
		.vertexBindingsMask = 0b1,
		.instanceDataBindingIndex = INSTANCE_DATA_BINDING,
	};
}
