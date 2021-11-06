#include "StaticPropMaterial.hpp"
#include "MeshDrawArgs.hpp"
#include "../RenderSettings.hpp"
#include "../WallShader.hpp"
#include "../../Settings.hpp"

#include <magic_enum.hpp>
#include <fstream>

static const eg::AssetFormat StaticPropMaterialAssetFormat { "StaticPropMaterial", 9 };

class StaticPropMaterialGenerator : public eg::AssetGenerator
{
public:
	bool Generate(eg::AssetGenerateContext& generateContext) override
	{
		std::string relSourcePath = generateContext.RelSourcePath();
		std::string sourcePath = generateContext.FileDependency(relSourcePath);
		std::ifstream stream(sourcePath);
		if (!stream)
			return false;
		
		YAML::Node rootYaml = YAML::Load(stream);
		
		const float roughnessMin = rootYaml["roughnessMin"].as<float>(0.0f);
		const float roughnessMax = rootYaml["roughnessMax"].as<float>(1.0f);
		const float texScale = rootYaml["textureScale"].as<float>(1.0f);
		const float texScaleX = rootYaml["textureScaleX"].as<float>(texScale);
		const float texScaleY = rootYaml["textureScaleY"].as<float>(texScale);
		const bool backfaceCull = rootYaml["backfaceCull"].as<bool>(true);
		const bool backfaceCullEd = rootYaml["backfaceCullEd"].as<bool>(false);
		const bool castShadows = rootYaml["castShadows"].as<bool>(true);
		const bool alphaTest = rootYaml["alphaTest"].as<bool>(false);
		
		QualityLevel minShadowQuality = QualityLevel::VeryLow;
		std::string minShadowQualityStr = rootYaml["minShadowQuality"].as<std::string>("");
		if (!minShadowQualityStr.empty())
		{
			if (std::optional<QualityLevel> levelOpt = magic_enum::enum_cast<QualityLevel>(minShadowQualityStr))
			{
				minShadowQuality = *levelOpt;
			}
			else
			{
				eg::Log(eg::LogLevel::Error, "as", "Unknown shadow quality level {} in {}", minShadowQualityStr, sourcePath);
			}
		}
		
		std::string albedoPath = rootYaml["albedo"].as<std::string>(std::string());
		std::string normalMapPath = rootYaml["normalMap"].as<std::string>(std::string());
		std::string miscMapPath = rootYaml["miscMap"].as<std::string>(std::string());
		
		if (albedoPath.empty() || normalMapPath.empty() || miscMapPath.empty())
		{
			eg::Log(eg::LogLevel::Error, "as", "Invalid static prop material '{0}', "
			        "missing one of 'albedo', 'normalMap', or 'miscMap'", relSourcePath);
			return false;
		}
		
		eg::BinWriteString(generateContext.outputStream, albedoPath);
		eg::BinWriteString(generateContext.outputStream, normalMapPath);
		eg::BinWriteString(generateContext.outputStream, miscMapPath);
		eg::BinWrite(generateContext.outputStream, roughnessMin);
		eg::BinWrite(generateContext.outputStream, roughnessMax);
		eg::BinWrite(generateContext.outputStream, texScaleX);
		eg::BinWrite(generateContext.outputStream, texScaleY);
		eg::BinWrite<uint8_t>(generateContext.outputStream, (uint8_t)backfaceCull | ((uint8_t)backfaceCullEd << 1));
		eg::BinWrite<uint8_t>(generateContext.outputStream, (uint8_t)castShadows);
		eg::BinWrite<uint8_t>(generateContext.outputStream, (uint8_t)minShadowQuality);
		eg::BinWrite<uint8_t>(generateContext.outputStream, (uint8_t)alphaTest);
		
		generateContext.AddLoadDependency(std::move(albedoPath));
		generateContext.AddLoadDependency(std::move(normalMapPath));
		generateContext.AddLoadDependency(std::move(miscMapPath));
		
		return true;
	}
};

struct
{
	//Indices are: [cull enablement][alpha test][array textures]
	eg::Pipeline pipelineEditor[2][2][2];
	eg::Pipeline pipelineGame[2][2][2];
	
	//Indices are: [cull enablement][alpha test]
	eg::Pipeline pipelinePLShadow[2][2];
	
	std::unique_ptr<StaticPropMaterial> wallMaterials[MAX_WALL_MATERIALS];
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
	pipelineCI.vertexAttributes[4] = { 1, eg::DataType::Float32, 4, offsetof(StaticPropMaterial::InstanceData, transform) + 0 };
	pipelineCI.vertexAttributes[5] = { 1, eg::DataType::Float32, 4, offsetof(StaticPropMaterial::InstanceData, transform) + 16 };
	pipelineCI.vertexAttributes[6] = { 1, eg::DataType::Float32, 4, offsetof(StaticPropMaterial::InstanceData, transform) + 32 };
	pipelineCI.vertexAttributes[7] = { 1, eg::DataType::Float32, 4, offsetof(StaticPropMaterial::InstanceData, textureRange) };
}

static void OnInit()
{
	const eg::ShaderModuleAsset& fs = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/StaticModel.fs.glsl");
	
	eg::GraphicsPipelineCreateInfo pipelineCI;
	StaticPropMaterial::InitializeForCommon3DVS(pipelineCI);
	pipelineCI.enableDepthWrite = true;
	pipelineCI.enableDepthTest = true;
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	pipelineCI.numColorAttachments = 2;
	
	auto InitializeVariants = [&] (std::string_view variantPrefix, eg::Pipeline out[2][2][2])
	{
		for (int enableCulling = 0; enableCulling < 2; enableCulling++)
		{
			pipelineCI.cullMode = enableCulling ? eg::CullMode::Back : eg::CullMode::None;
			for (uint32_t alphaTest = 0; alphaTest < 2; alphaTest++)
			{
				for (int textureArray = 0; textureArray < 2; textureArray++)
				{
					std::string variantName = eg::Concat({variantPrefix, textureArray ? "TexArray" : "Tex2D"});
					pipelineCI.fragmentShader = fs.GetVariant(variantName);
					
					eg::SpecializationConstantEntry specConstEntry = { 5, 0, sizeof(uint32_t) };
					pipelineCI.fragmentShader.specConstants = { &specConstEntry, 1 };
					pipelineCI.fragmentShader.specConstantsDataSize = sizeof(uint32_t);
					pipelineCI.fragmentShader.specConstantsData = &alphaTest;
					
					std::string label = eg::Concat({
						"StaticProp:",
						variantName,
						alphaTest ? ":AT0" : ":AT1",
						enableCulling ? ":C1" : ":C0"
					});
					pipelineCI.label = label.c_str();
					
					out[enableCulling][alphaTest][textureArray] = eg::Pipeline::Create(pipelineCI);
				}
			}
		}
	};
	
	InitializeVariants("VGame", staticPropMaterialGlobals.pipelineGame);
	
	pipelineCI.numColorAttachments = 1;
	InitializeVariants("VEditor", staticPropMaterialGlobals.pipelineEditor);
	
	const eg::ShaderModuleAsset& plsfs = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/PointLightShadow.fs.glsl");
	
	eg::GraphicsPipelineCreateInfo plsPipelineCI;
	plsPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Common3D-PLShadow.vs.glsl").DefaultVariant();
	
	eg::FramebufferFormatHint plsFormatHint;
	plsFormatHint.sampleCount = 1;
	plsFormatHint.depthStencilFormat = PointLightShadowMapper::SHADOW_MAP_FORMAT;
	
	plsPipelineCI.enableDepthWrite = true;
	plsPipelineCI.enableDepthTest = true;
	plsPipelineCI.frontFaceCCW = eg::CurrentGraphicsAPI() == eg::GraphicsAPI::Vulkan;
	plsPipelineCI.vertexBindings[0] = { sizeof(eg::StdVertex), eg::InputRate::Vertex };
	plsPipelineCI.vertexBindings[1] = { sizeof(StaticPropMaterial::InstanceData), eg::InputRate::Instance };
	plsPipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, offsetof(eg::StdVertex, position) };
	plsPipelineCI.vertexAttributes[1] = { 0, eg::DataType::Float32, 2, offsetof(eg::StdVertex, texCoord) };
	plsPipelineCI.vertexAttributes[2] = { 1, eg::DataType::Float32, 4, offsetof(StaticPropMaterial::InstanceData, transform) + 0 };
	plsPipelineCI.vertexAttributes[3] = { 1, eg::DataType::Float32, 4, offsetof(StaticPropMaterial::InstanceData, transform) + 16 };
	plsPipelineCI.vertexAttributes[4] = { 1, eg::DataType::Float32, 4, offsetof(StaticPropMaterial::InstanceData, transform) + 32 };
	plsPipelineCI.vertexAttributes[5] = { 1, eg::DataType::Float32, 4, offsetof(StaticPropMaterial::InstanceData, textureRange) };
	for (int enableCulling = 0; enableCulling < 2; enableCulling++)
	{
		pipelineCI.cullMode = enableCulling ? eg::CullMode::Back : eg::CullMode::None;
		for (int alphaTest = 0; alphaTest < 2; alphaTest++)
		{
			std::string_view variantName = alphaTest ? "VAlphaTest" : "VNoAlphaTest";
			std::string label = eg::Concat({"StaticPropPLS:", variantName, enableCulling ? ":Cull" : ":NoCull"});
			plsPipelineCI.fragmentShader = plsfs.GetVariant(variantName);
			plsPipelineCI.label = label.c_str();
			staticPropMaterialGlobals.pipelinePLShadow[enableCulling][alphaTest] = eg::Pipeline::Create(plsPipelineCI);
			staticPropMaterialGlobals.pipelinePLShadow[enableCulling][alphaTest].FramebufferFormatHint(plsFormatHint);
		}
	}
}

static void OnShutdown()
{
	staticPropMaterialGlobals = {};
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

bool StaticPropMaterial::AssetLoader(const eg::AssetLoadContext& loadContext)
{
	eg::MemoryStreambuf memoryStreambuf(loadContext.Data());
	std::istream stream(&memoryStreambuf);
	
	std::string albedoTextureName = eg::BinReadString(stream);
	std::string normalMapTextureName = eg::BinReadString(stream);
	std::string miscMapTextureName = eg::BinReadString(stream);
	
	std::string albedoTexturePath = eg::Concat({ loadContext.DirPath(), albedoTextureName} );
	std::string normalMapTexturePath = eg::Concat({ loadContext.DirPath(), normalMapTextureName} );
	std::string miscMapTexturePath = eg::Concat({ loadContext.DirPath(), miscMapTextureName} );
	
	StaticPropMaterial& material = loadContext.CreateResult<StaticPropMaterial>();
	material.m_albedoTexture = &eg::GetAsset<eg::Texture>(albedoTexturePath);
	material.m_normalMapTexture = &eg::GetAsset<eg::Texture>(normalMapTexturePath);
	material.m_miscMapTexture = &eg::GetAsset<eg::Texture>(miscMapTexturePath);
	
	material.m_roughnessMin = eg::BinRead<float>(stream);
	material.m_roughnessMax = eg::BinRead<float>(stream);
	material.m_textureScale.x = 1.0f / eg::BinRead<float>(stream);
	material.m_textureScale.y = 1.0f / eg::BinRead<float>(stream);
	uint8_t backfaceCullFlags = eg::BinRead<uint8_t>(stream);
	material.m_backfaceCull = (backfaceCullFlags & 1) != 0;
	material.m_backfaceCullEditor = (backfaceCullFlags & 2) != 0;
	material.m_castShadows = eg::BinRead<uint8_t>(stream);
	material.m_minShadowQuality = (QualityLevel)eg::BinRead<uint8_t>(stream);
	material.m_alphaTest = eg::BinRead<uint8_t>(stream);
	
	return true;
}

void StaticPropMaterial::InitAssetTypes()
{
	eg::RegisterAssetGenerator<StaticPropMaterialGenerator>("StaticPropMaterial", StaticPropMaterialAssetFormat);
	eg::RegisterAssetLoader("StaticPropMaterial", &StaticPropMaterial::AssetLoader, StaticPropMaterialAssetFormat);
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
		return staticPropMaterialGlobals.pipelineGame[m_backfaceCull][m_alphaTest][m_textureLayer.has_value()];
	case MeshDrawMode::Editor:
		return staticPropMaterialGlobals.pipelineEditor[m_backfaceCullEditor][m_alphaTest][m_textureLayer.has_value()];
	case MeshDrawMode::PointLightShadow:
		return staticPropMaterialGlobals.pipelinePLShadow[m_backfaceCullEditor][m_alphaTest];
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
			cmdCtx.BindTexture(*m_albedoTexture, 0, 1, nullptr);
		return true;
	}
	
	if (!m_descriptorsInitialized)
	{
		m_descriptorSet = eg::DescriptorSet(GetPipeline(MeshDrawMode::Game), 0);
		m_descriptorsInitialized = true;
		
		m_descriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
		m_descriptorSet.BindTexture(*m_albedoTexture, 1, nullptr);
		m_descriptorSet.BindTexture(*m_normalMapTexture, 2, nullptr);
		m_descriptorSet.BindTexture(*m_miscMapTexture, 3, nullptr);
	}
	
	if (mDrawArgs->drawMode == MeshDrawMode::Editor || mDrawArgs->drawMode == MeshDrawMode::Game)
	{
		cmdCtx.BindDescriptorSet(m_descriptorSet, 0);
		
		const float pushConstants[] =
		{
			m_roughnessMin, m_roughnessMax,
			m_textureScale.x, m_textureScale.y,
			(float)m_textureLayer.value_or(0)
		};
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
		staticPropMaterialGlobals.wallMaterials[index] = std::move(material);
	}
	return *staticPropMaterialGlobals.wallMaterials[index];
}
