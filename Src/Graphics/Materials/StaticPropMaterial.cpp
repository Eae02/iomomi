#include "StaticPropMaterial.hpp"

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
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Common3D.vs.glsl").DefaultVariant();

	pipelineCI.vertexBindings[0] = { sizeof(eg::StdVertex), eg::InputRate::Vertex };
	pipelineCI.vertexBindings[1] = { sizeof(StaticPropMaterial::InstanceData), eg::InputRate::Instance };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, offsetof(eg::StdVertex, position) };
	pipelineCI.vertexAttributes[1] = { 0, eg::DataType::Float32, 2, offsetof(eg::StdVertex, texCoord) };
	pipelineCI.vertexAttributes[2] = { 0, eg::DataType::SInt8Norm, 3, offsetof(eg::StdVertex, normal) };
	pipelineCI.vertexAttributes[3] = { 0, eg::DataType::SInt8Norm, 3, offsetof(eg::StdVertex, tangent) };
	pipelineCI.vertexAttributes[4] = { 1, eg::DataType::Float32, 4,
		                               offsetof(StaticPropMaterial::InstanceData, transform) + 0 };
	pipelineCI.vertexAttributes[5] = { 1, eg::DataType::Float32, 4,
		                               offsetof(StaticPropMaterial::InstanceData, transform) + 16 };
	pipelineCI.vertexAttributes[6] = { 1, eg::DataType::Float32, 4,
		                               offsetof(StaticPropMaterial::InstanceData, transform) + 32 };
	pipelineCI.vertexAttributes[7] = { 1, eg::DataType::Float32, 4,
		                               offsetof(StaticPropMaterial::InstanceData, textureRange) };
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
				pipelineCI.fragmentShader = fs.GetVariant(variantName);

				eg::SpecializationConstantEntry specConstEntry = { 5, 0, sizeof(uint32_t) };
				pipelineCI.fragmentShader.specConstants = { &specConstEntry, 1 };
				pipelineCI.fragmentShader.specConstantsDataSize = sizeof(uint32_t);
				pipelineCI.fragmentShader.specConstantsData = &alphaTest;

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
	pipelineCI.colorAttachmentFormats[0] = eg::Format::DefaultColor;
	pipelineCI.depthAttachmentFormat = eg::Format::DefaultDepthStencil;
	InitializeVariants("VEditor", staticPropMaterialGlobals.pipelineEditor);

	const eg::ShaderModuleAsset& plsfs = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/PointLightShadow.fs.glsl");

	eg::GraphicsPipelineCreateInfo plsPipelineCI;
	plsPipelineCI.vertexShader =
		eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Common3D-PLShadow.vs.glsl").DefaultVariant();

	plsPipelineCI.enableDepthWrite = true;
	plsPipelineCI.enableDepthTest = true;
	plsPipelineCI.frontFaceCCW = PointLightShadowMapper::FlippedLightMatrix();
	plsPipelineCI.numColorAttachments = 0;
	plsPipelineCI.depthAttachmentFormat = PointLightShadowMapper::SHADOW_MAP_FORMAT;

	plsPipelineCI.vertexBindings[0] = { sizeof(eg::StdVertex), eg::InputRate::Vertex };
	plsPipelineCI.vertexBindings[1] = { sizeof(StaticPropMaterial::InstanceData), eg::InputRate::Instance };
	plsPipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, offsetof(eg::StdVertex, position) };
	plsPipelineCI.vertexAttributes[1] = { 0, eg::DataType::Float32, 2, offsetof(eg::StdVertex, texCoord) };
	plsPipelineCI.vertexAttributes[2] = { 1, eg::DataType::Float32, 4,
		                                  offsetof(StaticPropMaterial::InstanceData, transform) + 0 };
	plsPipelineCI.vertexAttributes[3] = { 1, eg::DataType::Float32, 4,
		                                  offsetof(StaticPropMaterial::InstanceData, transform) + 16 };
	plsPipelineCI.vertexAttributes[4] = { 1, eg::DataType::Float32, 4,
		                                  offsetof(StaticPropMaterial::InstanceData, transform) + 32 };
	plsPipelineCI.vertexAttributes[5] = { 1, eg::DataType::Float32, 4,
		                                  offsetof(StaticPropMaterial::InstanceData, textureRange) };
	plsPipelineCI.cullMode = std::nullopt;
	for (int alphaTest = 0; alphaTest < 2; alphaTest++)
	{
		std::string_view variantName = alphaTest ? "VAlphaTest" : "VNoAlphaTest";
		std::string label = eg::Concat({ "StaticPropPLS:", variantName });
		plsPipelineCI.fragmentShader = plsfs.GetVariant(variantName);
		plsPipelineCI.label = label.c_str();
		staticPropMaterialGlobals.pipelinePLShadow[alphaTest] = eg::Pipeline::Create(plsPipelineCI);
	}
}

static void OnShutdown()
{
	staticPropMaterialGlobals = {};
}

EG_ON_SHUTDOWN(OnShutdown)

void StaticPropMaterial::CreateDescriptorSet()
{
	m_descriptorSet = eg::DescriptorSet(GetPipeline(MeshDrawMode::Game), 0);

	m_descriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	m_descriptorSet.BindTexture(*m_albedoTexture, 1, &commonTextureSampler);
	m_descriptorSet.BindTexture(*m_normalMapTexture, 2, &commonTextureSampler);
	m_descriptorSet.BindTexture(*m_miscMapTexture, 3, &commonTextureSampler);
}

size_t StaticPropMaterial::PipelineHash() const
{
	size_t h = typeid(StaticPropMaterial).hash_code();
	eg::HashAppend(h, m_textureLayer.value_or(UINT32_MAX));
	eg::HashAppend(h, m_backfaceCull);
	eg::HashAppend(h, m_backfaceCullEditor);
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

	bool backfaceCull = mDrawArgs->drawMode == MeshDrawMode::Editor ? m_backfaceCullEditor : m_backfaceCull;
	cmdCtx.SetCullMode(backfaceCull ? eg::CullMode::Back : eg::CullMode::None);

	if (mDrawArgs->drawMode == MeshDrawMode::PointLightShadow)
	{
		mDrawArgs->plShadowRenderArgs->SetPushConstants();
	}

	return true;
}

bool StaticPropMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	MeshDrawArgs* mDrawArgs = static_cast<MeshDrawArgs*>(drawArgs);

	if (mDrawArgs->drawMode == MeshDrawMode::PointLightShadow)
	{
		if (!m_castShadows || settings.shadowQuality < m_minShadowQuality)
			return false;
		if (m_alphaTest)
			cmdCtx.BindTexture(*m_albedoTexture, 0, 1, &linearRepeatSampler);
		return true;
	}

	if (mDrawArgs->drawMode == MeshDrawMode::Editor || mDrawArgs->drawMode == MeshDrawMode::Game)
	{
		cmdCtx.BindDescriptorSet(m_descriptorSet, 0);

		const float pushConstants[] = { m_roughnessMin, m_roughnessMax, m_textureScale.x, m_textureScale.y,
			                            (float)m_textureLayer.value_or(0) };
		cmdCtx.PushConstants(0, sizeof(float) * (m_textureLayer.has_value() ? 5 : 4), pushConstants);
	}

	return true;
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
		material->m_albedoTexture = &eg::GetAsset<eg::Texture>("WallTextures/Albedo");
		material->m_normalMapTexture = &eg::GetAsset<eg::Texture>("WallTextures/NormalMap");
		material->m_miscMapTexture = &eg::GetAsset<eg::Texture>("WallTextures/MiscMap");
		material->m_textureLayer = index - 1;
		material->m_roughnessMin = wallMaterials[index].minRoughness;
		material->m_roughnessMax = wallMaterials[index].maxRoughness;
		material->m_textureScale = glm::vec2(1.0f / wallMaterials[index].textureScale);
		material->m_backfaceCullEditor = true;
		material->m_alphaTest = false;
		material->CreateDescriptorSet();
		staticPropMaterialGlobals.wallMaterials[index] = std::move(material);
	}
	return *staticPropMaterialGlobals.wallMaterials[index];
}
