#include "StaticPropMaterial.hpp"
#include "../../Editor/EditorGraphics.hpp"

#include <fstream>
#include <magic_enum/magic_enum.hpp>

#include "../../Settings.hpp"
#include "../RenderSettings.hpp"
#include "../WallShader.hpp"
#include "MeshDrawArgs.hpp"

struct
{
	// Indices are: [alpha test][array textures]
	eg::Pipeline pipelineEditor[2][2];
	eg::Pipeline pipelineGame[2][2];

	// Indices are: [alpha test]
	eg::Pipeline pipelinePLShadow[2];

	std::unique_ptr<StaticPropMaterial> wallMaterials[MAX_WALL_MATERIALS];

	bool initialized = false;
} staticPropMaterialGlobals;

void StaticPropMaterial::InitializeForCommon3DVS(eg::GraphicsPipelineCreateInfo& pipelineCI)
{
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Common3D.vs.glsl").ToStageInfo();

	InitPipelineVertexStateSoaPXNT(pipelineCI);

	constexpr uint32_t FIRST_INSTANCE_DATA_ATTRIBUTE = 4;
	InitializeInstanceDataVertexInput(pipelineCI, FIRST_INSTANCE_DATA_ATTRIBUTE);
}

void StaticPropMaterial::InitializeInstanceDataVertexInput(
	eg::GraphicsPipelineCreateInfo& pipelineCI, uint32_t firstAttribute
)
{
	pipelineCI.vertexBindings[VERTEX_BINDING_INSTANCE_DATA] = {
		sizeof(StaticPropMaterial::InstanceData),
		eg::InputRate::Instance,
	};

	pipelineCI.vertexAttributes[firstAttribute] = {
		VERTEX_BINDING_INSTANCE_DATA,
		eg::DataType::Float32,
		4,
		offsetof(StaticPropMaterial::InstanceData, transform) + 0,
	};
	pipelineCI.vertexAttributes[firstAttribute + 1] = {
		VERTEX_BINDING_INSTANCE_DATA,
		eg::DataType::Float32,
		4,
		offsetof(StaticPropMaterial::InstanceData, transform) + 16,
	};
	pipelineCI.vertexAttributes[firstAttribute + 2] = {
		VERTEX_BINDING_INSTANCE_DATA,
		eg::DataType::Float32,
		4,
		offsetof(StaticPropMaterial::InstanceData, transform) + 32,
	};
	pipelineCI.vertexAttributes[firstAttribute + 3] = {
		VERTEX_BINDING_INSTANCE_DATA,
		eg::DataType::Float32,
		4,
		offsetof(StaticPropMaterial::InstanceData, textureRange),
	};
}

void StaticPropMaterial::LazyInitGlobals()
{
	if (staticPropMaterialGlobals.initialized)
		return;
	staticPropMaterialGlobals.initialized = true;

	const eg::ShaderModuleAsset& fs = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/StaticModel.fs.glsl");

	eg::GraphicsPipelineCreateInfo pipelineCI;
	StaticPropMaterial::InitializeForCommon3DVS(pipelineCI);
	pipelineCI.enableDepthWrite = true;
	pipelineCI.enableDepthTest = true;
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	pipelineCI.cullMode = std::nullopt;

	auto InitializeVariants = [&](std::string_view variantPrefix, eg::Pipeline out[2][2])
	{
		for (uint32_t alphaTest = 0; alphaTest < 2; alphaTest++)
		{
			for (int textureArray = 0; textureArray < 2; textureArray++)
			{
				std::string variantName = eg::Concat({ variantPrefix, textureArray ? "TexArray" : "Tex2D" });
				pipelineCI.fragmentShader = fs.ToStageInfo(variantName);

				eg::SpecializationConstantEntry specConstEntry = { 5, alphaTest };
				pipelineCI.fragmentShader.specConstants = { &specConstEntry, 1 };

				std::string label = eg::Concat({ "StaticProp:", variantName, alphaTest ? ":AT0" : ":AT1" });
				pipelineCI.label = label.c_str();

				out[alphaTest][textureArray] = eg::Pipeline::Create(pipelineCI);
			}
		}
	};

	pipelineCI.numColorAttachments = 2;
	pipelineCI.colorAttachmentFormats[0] = GB_COLOR_FORMAT;
	pipelineCI.colorAttachmentFormats[1] = GB_COLOR_FORMAT;
	pipelineCI.depthAttachmentFormat = GB_DEPTH_FORMAT;
	InitializeVariants("VGame", staticPropMaterialGlobals.pipelineGame);

	pipelineCI.numColorAttachments = 1;
	pipelineCI.colorAttachmentFormats[0] = EDITOR_COLOR_FORMAT;
	pipelineCI.depthAttachmentFormat = EDITOR_DEPTH_FORMAT;
	InitializeVariants("VEditor", staticPropMaterialGlobals.pipelineEditor);

	const eg::ShaderModuleAsset& plsvs = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Common3D-PLShadow.vs.glsl");
	const eg::ShaderModuleAsset& plsfs = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/PointLightShadow.fs.glsl");

	eg::GraphicsPipelineCreateInfo plsPipelineCI;

	plsPipelineCI.enableDepthWrite = true;
	plsPipelineCI.enableDepthTest = true;
	plsPipelineCI.frontFaceCCW = PointLightShadowMapper::FlippedLightMatrix();
	plsPipelineCI.numColorAttachments = 0;
	plsPipelineCI.depthAttachmentFormat = PointLightShadowMapper::SHADOW_MAP_FORMAT;
	plsPipelineCI.cullMode = std::nullopt;
	plsPipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	plsPipelineCI.setBindModes[1] = eg::BindMode::DescriptorSet;
	plsPipelineCI.descriptorSetBindings[0] = PointLightShadowDrawArgs::PARAMETERS_DS_BINDINGS;
	plsPipelineCI.vertexBindings[VERTEX_BINDING_POSITION] = { VERTEX_STRIDE_POSITION, eg::InputRate::Vertex };
	plsPipelineCI.vertexAttributes[0] = { VERTEX_BINDING_POSITION, eg::DataType::Float32, 3, 0 };
	constexpr uint32_t PLS_FIRST_INSTANCE_DATA_ATTRIBUTE = 2;
	InitializeInstanceDataVertexInput(plsPipelineCI, PLS_FIRST_INSTANCE_DATA_ATTRIBUTE);
	for (int alphaTest = 0; alphaTest < 2; alphaTest++)
	{
		if (alphaTest)
		{
			plsPipelineCI.vertexBindings[VERTEX_BINDING_TEXCOORD] = { VERTEX_STRIDE_TEXCOORD, eg::InputRate::Vertex };
			plsPipelineCI.vertexAttributes[1] = { VERTEX_BINDING_TEXCOORD, eg::DataType::Float32, 2, 0 };
		}

		std::string_view variantName = alphaTest ? "VAlphaTest" : "VNoAlphaTest";
		std::string label = eg::Concat({ "StaticPropPLS:", variantName });
		plsPipelineCI.vertexShader = plsvs.ToStageInfo(variantName);
		plsPipelineCI.fragmentShader = plsfs.ToStageInfo(variantName);
		plsPipelineCI.label = label.c_str();
		staticPropMaterialGlobals.pipelinePLShadow[alphaTest] = eg::Pipeline::Create(plsPipelineCI);
	}
}

static void OnShutdown()
{
	staticPropMaterialGlobals = {};
}

EG_ON_SHUTDOWN(OnShutdown)

void StaticPropMaterial::CreateDescriptorSetAndParamsBuffer()
{
	const float parametersData[5] = {
		m_roughnessMin, m_roughnessMax, m_textureScale.x, m_textureScale.y, (float)m_textureLayer.value_or(0),
	};

	m_parametersBuffer = eg::Buffer(eg::BufferFlags::UniformBuffer, sizeof(parametersData), parametersData);

	m_descriptorSet = eg::DescriptorSet(GetPipeline(MeshDrawMode::Game), 0);

	m_descriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0);
	m_descriptorSet.BindTexture(*m_albedoTexture, 1, &commonTextureSampler);
	m_descriptorSet.BindTexture(*m_normalMapTexture, 2, &commonTextureSampler);
	m_descriptorSet.BindTexture(*m_miscMapTexture, 3, &commonTextureSampler);
	m_descriptorSet.BindUniformBuffer(m_parametersBuffer, 4);

	if (m_alphaTest)
	{
		m_plShadowDescriptorSet = eg::DescriptorSet(GetPipeline(MeshDrawMode::PointLightShadow), 1);
		m_plShadowDescriptorSet.BindTexture(*m_albedoTexture, 0, &commonTextureSampler);
	}
}

size_t StaticPropMaterial::PipelineHash() const
{
	size_t h = typeid(StaticPropMaterial).hash_code();
	eg::HashAppend(h, m_textureLayer.value_or(UINT32_MAX));
	eg::HashAppend(h, m_alphaTest);
	return h;
}

eg::PipelineRef StaticPropMaterial::GetPipeline(MeshDrawMode drawMode) const
{
	switch (drawMode)
	{
	case MeshDrawMode::Game:
		return staticPropMaterialGlobals.pipelineGame[m_alphaTest][m_textureLayer.has_value()];
	case MeshDrawMode::Editor:
		return staticPropMaterialGlobals.pipelineEditor[m_alphaTest][m_textureLayer.has_value()];
	case MeshDrawMode::PointLightShadow:
		return staticPropMaterialGlobals.pipelinePLShadow[m_alphaTest];
	default:
		return eg::PipelineRef();
	}
}

bool StaticPropMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	MeshDrawArgs* mDrawArgs = static_cast<MeshDrawArgs*>(drawArgs);

	eg::PipelineRef pipeline = GetPipeline(mDrawArgs->drawMode);
	if (!pipeline.handle)
		return false;
	cmdCtx.BindPipeline(pipeline);

	if (mDrawArgs->drawMode == MeshDrawMode::PointLightShadow)
	{
		mDrawArgs->plShadowRenderArgs->BindParametersDescriptorSet(cmdCtx, 0);
	}

	return true;
}

bool StaticPropMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	MeshDrawArgs* mDrawArgs = static_cast<MeshDrawArgs*>(drawArgs);

	bool backfaceCull = mDrawArgs->drawMode == MeshDrawMode::Editor ? m_backfaceCullEditor : m_backfaceCull;
	cmdCtx.SetCullMode(backfaceCull ? eg::CullMode::Back : eg::CullMode::None);

	if (mDrawArgs->drawMode == MeshDrawMode::PointLightShadow)
	{
		if (!m_castShadows || settings.shadowQuality < m_minShadowQuality)
			return false;
		if (m_alphaTest)
			cmdCtx.BindDescriptorSet(m_plShadowDescriptorSet, 1);
		return true;
	}

	if (mDrawArgs->drawMode == MeshDrawMode::Editor || mDrawArgs->drawMode == MeshDrawMode::Game)
	{
		cmdCtx.BindDescriptorSet(m_descriptorSet, 0);
	}

	return true;
}

eg::IMaterial::VertexInputConfiguration StaticPropMaterial::GetVertexInputConfiguration(const void* drawArgs) const
{
	return GetVertexInputConfiguration(m_alphaTest, reinterpret_cast<const MeshDrawArgs*>(drawArgs)->drawMode);
}

eg::IMaterial::VertexInputConfiguration StaticPropMaterial::GetVertexInputConfiguration(
	bool alphaTest, MeshDrawMode drawMode
)
{
	if (drawMode == MeshDrawMode::PointLightShadow)
	{
		return VertexInputConfiguration{
			.vertexBindingsMask = alphaTest ? 0b11u : 0b1u, // enable texture coordinate only if using alpha testing
			.instanceDataBindingIndex = VERTEX_BINDING_INSTANCE_DATA,
		};
	}
	else
	{
		return VertexInputConfig_SoaPXNTI;
	}
}

bool StaticPropMaterial::CheckInstanceDataType(const std::type_info* instanceDataType) const
{
	return instanceDataType == &typeid(InstanceData);
}

const StaticPropMaterial& StaticPropMaterial::GetFromWallMaterial(uint32_t index)
{
	EG_ASSERT(index != 0)
	LazyInitGlobals();
	if (staticPropMaterialGlobals.wallMaterials[index] == nullptr)
	{
		auto material = std::make_unique<StaticPropMaterial>();
		material->m_albedoTexture = &eg::GetAsset<eg::Texture>("Textures/Wall/Albedo");
		material->m_normalMapTexture = &eg::GetAsset<eg::Texture>("Textures/Wall/NormalMap");
		material->m_miscMapTexture = &eg::GetAsset<eg::Texture>("Textures/Wall/MiscMap");
		material->m_textureLayer = index - 1;
		material->m_roughnessMin = wallMaterials[index].minRoughness;
		material->m_roughnessMax = wallMaterials[index].maxRoughness;
		material->m_textureScale = glm::vec2(1.0f / wallMaterials[index].textureScale);
		material->m_backfaceCullEditor = true;
		material->m_alphaTest = false;
		material->CreateDescriptorSetAndParamsBuffer();
		staticPropMaterialGlobals.wallMaterials[index] = std::move(material);
	}
	return *staticPropMaterialGlobals.wallMaterials[index];
}
