#include "StaticPropMaterial.hpp"
#include "MeshDrawArgs.hpp"
#include "../RenderSettings.hpp"
#include "../WallShader.hpp"
#include "../../Settings.hpp"

#include <fstream>

static const eg::AssetFormat StaticPropMaterialAssetFormat { "StaticPropMaterial", 7 };

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
		const bool enableSSR = rootYaml["enableSSR"].as<bool>(true);
		
		std::string minShadowQualityString = rootYaml["minShadowQuality"].as<std::string>("vlow");
		QualityLevel minShadowQuality = QualityLevel::VeryLow;
		DecodeQualityLevel(minShadowQualityString, minShadowQuality);
		
		std::string albedoPath = rootYaml["albedo"].as<std::string>(std::string());
		std::string normalMapPath = rootYaml["normalMap"].as<std::string>(std::string());
		std::string miscMapPath = rootYaml["miscMap"].as<std::string>(std::string());
		
		if (albedoPath.empty())
		{
			eg::Log(eg::LogLevel::Error, "as", "Invalid static prop material '{0}', "
			        "missing one of 'albedo', 'normalMap', or 'miscMap'", relSourcePath);
			return false;
		}
		
		uint32_t objectFlags = 0;
		if (!enableSSR)
			objectFlags |= ObjectRenderFlags::NoSSR;
		
		eg::BinWriteString(generateContext.outputStream, albedoPath);
		eg::BinWriteString(generateContext.outputStream, normalMapPath);
		eg::BinWriteString(generateContext.outputStream, miscMapPath);
		eg::BinWrite(generateContext.outputStream, roughnessMin);
		eg::BinWrite(generateContext.outputStream, roughnessMax);
		eg::BinWrite(generateContext.outputStream, texScaleX);
		eg::BinWrite(generateContext.outputStream, texScaleY);
		eg::BinWrite<uint8_t>(generateContext.outputStream, (uint8_t)backfaceCull | ((uint8_t)backfaceCullEd << 1));
		eg::BinWrite<uint8_t>(generateContext.outputStream, (uint8_t)castShadows);
		eg::BinWrite<uint8_t>(generateContext.outputStream, (uint8_t)objectFlags);
		eg::BinWrite<uint8_t>(generateContext.outputStream, (uint8_t)minShadowQuality);
		
		generateContext.AddLoadDependency(std::move(albedoPath));
		generateContext.AddLoadDependency(std::move(normalMapPath));
		generateContext.AddLoadDependency(std::move(miscMapPath));
		
		return true;
	}
};

static eg::Pipeline staticPropPipelineEditor[2];
static eg::Pipeline staticPropPipelineGame[2];
static eg::Pipeline staticPropPipelineFlags[2];
static eg::Pipeline staticPropPipelinePLShadow[2];

static std::array<std::unique_ptr<StaticPropMaterial>, MAX_WALL_MATERIALS> staticPropWallMaterials;

eg::PipelineRef StaticPropMaterial::FlagsPipelineBackCull;
eg::PipelineRef StaticPropMaterial::FlagsPipelineNoCull;

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
	pipelineCI.vertexAttributes[7] = { 1, eg::DataType::Float32, 2, offsetof(StaticPropMaterial::InstanceData, textureScale) };
}

static void OnInit()
{
	eg::GraphicsPipelineCreateInfo pipelineCI;
	StaticPropMaterial::InitializeForCommon3DVS(pipelineCI);
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/StaticModel.fs.glsl").DefaultVariant();
	pipelineCI.enableDepthWrite = true;
	pipelineCI.enableDepthTest = true;
	pipelineCI.cullMode = eg::CullMode::Back;
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	pipelineCI.numColorAttachments = 2;
	pipelineCI.label = "StaticPropGame";
	staticPropPipelineGame[1] = eg::Pipeline::Create(pipelineCI);
	staticPropPipelineGame[1].FramebufferFormatHint(DeferredRenderer::GEOMETRY_FB_FORMAT);
	
	pipelineCI.cullMode = eg::CullMode::None;
	pipelineCI.label = "StaticPropGameNoCull";
	staticPropPipelineGame[0] = eg::Pipeline::Create(pipelineCI);
	staticPropPipelineGame[0].FramebufferFormatHint(DeferredRenderer::GEOMETRY_FB_FORMAT);
	
	pipelineCI.numColorAttachments = 1;
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/StaticModel-Editor.fs.glsl").DefaultVariant();
	pipelineCI.cullMode = eg::CullMode::Back;
	pipelineCI.label = "StaticPropEditor";
	staticPropPipelineEditor[1] = eg::Pipeline::Create(pipelineCI);
	staticPropPipelineEditor[1].FramebufferFormatHint(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil);
	
	pipelineCI.cullMode = eg::CullMode::None;
	pipelineCI.label = "StaticPropEditorNoCull";
	staticPropPipelineEditor[0] = eg::Pipeline::Create(pipelineCI);
	staticPropPipelineEditor[0].FramebufferFormatHint(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil);
	
	eg::GraphicsPipelineCreateInfo flagsPipelineCI;
	flagsPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/ObjectFlags.vs.glsl").DefaultVariant();
	flagsPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/ObjectFlags.fs.glsl").DefaultVariant();
	flagsPipelineCI.enableDepthTest = true;
	flagsPipelineCI.enableDepthWrite = false;
	flagsPipelineCI.depthCompare = eg::CompareOp::LessOrEqual;
	flagsPipelineCI.cullMode = eg::CullMode::Back;
	flagsPipelineCI.vertexBindings[0] = { sizeof(eg::StdVertex), eg::InputRate::Vertex };
	flagsPipelineCI.vertexBindings[1] = { sizeof(StaticPropMaterial::InstanceData), eg::InputRate::Instance };
	flagsPipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, offsetof(eg::StdVertex, position) };
	flagsPipelineCI.vertexAttributes[1] = { 1, eg::DataType::Float32, 4, offsetof(StaticPropMaterial::InstanceData, transform) + 0 };
	flagsPipelineCI.vertexAttributes[2] = { 1, eg::DataType::Float32, 4, offsetof(StaticPropMaterial::InstanceData, transform) + 16 };
	flagsPipelineCI.vertexAttributes[3] = { 1, eg::DataType::Float32, 4, offsetof(StaticPropMaterial::InstanceData, transform) + 32 };
	flagsPipelineCI.numColorAttachments = 1;
	flagsPipelineCI.label = "StaticPropFlags";
	staticPropPipelineFlags[1] = eg::Pipeline::Create(flagsPipelineCI);
	
	flagsPipelineCI.cullMode = eg::CullMode::None;
	flagsPipelineCI.label = "StaticPropFlagsNoCull";
	staticPropPipelineFlags[0] = eg::Pipeline::Create(flagsPipelineCI);
	
	StaticPropMaterial::FlagsPipelineNoCull = staticPropPipelineFlags[0];
	StaticPropMaterial::FlagsPipelineBackCull = staticPropPipelineFlags[1];
	
	
	eg::GraphicsPipelineCreateInfo plsPipelineCI;
	plsPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Common3D-PLShadow.vs.glsl").DefaultVariant();
	plsPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/PointLightShadow.fs.glsl").GetVariant("VAlbedo");
	plsPipelineCI.enableDepthWrite = true;
	plsPipelineCI.enableDepthTest = true;
	plsPipelineCI.frontFaceCCW = eg::CurrentGraphicsAPI() == eg::GraphicsAPI::Vulkan;
	plsPipelineCI.cullMode = eg::CullMode::Back;
	plsPipelineCI.vertexBindings[0] = { sizeof(eg::StdVertex), eg::InputRate::Vertex };
	plsPipelineCI.vertexBindings[1] = { sizeof(StaticPropMaterial::InstanceData), eg::InputRate::Instance };
	plsPipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, offsetof(eg::StdVertex, position) };
	plsPipelineCI.vertexAttributes[1] = { 0, eg::DataType::Float32, 2, offsetof(eg::StdVertex, texCoord) };
	plsPipelineCI.vertexAttributes[2] = { 1, eg::DataType::Float32, 4, offsetof(StaticPropMaterial::InstanceData, transform) + 0 };
	plsPipelineCI.vertexAttributes[3] = { 1, eg::DataType::Float32, 4, offsetof(StaticPropMaterial::InstanceData, transform) + 16 };
	plsPipelineCI.vertexAttributes[4] = { 1, eg::DataType::Float32, 4, offsetof(StaticPropMaterial::InstanceData, transform) + 32 };
	plsPipelineCI.vertexAttributes[5] = { 1, eg::DataType::Float32, 2, offsetof(StaticPropMaterial::InstanceData, textureScale) };
	plsPipelineCI.label = "StaticPropPLS";
	staticPropPipelinePLShadow[1] = eg::Pipeline::Create(plsPipelineCI);
	
	plsPipelineCI.cullMode = eg::CullMode::None;
	plsPipelineCI.label = "StaticPropPLSNoCull";
	staticPropPipelinePLShadow[0] = eg::Pipeline::Create(plsPipelineCI);
	
	eg::FramebufferFormatHint plsFormatHint;
	plsFormatHint.sampleCount = 1;
	plsFormatHint.depthStencilFormat = PointLightShadowMapper::SHADOW_MAP_FORMAT;
	staticPropPipelinePLShadow[1].FramebufferFormatHint(plsFormatHint);
	staticPropPipelinePLShadow[0].FramebufferFormatHint(plsFormatHint);
}

static void OnShutdown()
{
	staticPropPipelineEditor[0].Destroy();
	staticPropPipelineEditor[1].Destroy();
	staticPropPipelineGame[0].Destroy();
	staticPropPipelineGame[1].Destroy();
	staticPropPipelineFlags[0].Destroy();
	staticPropPipelineFlags[1].Destroy();
	staticPropPipelinePLShadow[0].Destroy();
	staticPropPipelinePLShadow[1].Destroy();
	staticPropWallMaterials = { };
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
	material.m_objectFlags = eg::BinRead<uint8_t>(stream);
	material.m_minShadowQuality = (QualityLevel)eg::BinRead<uint8_t>(stream);
	
	return true;
}

void StaticPropMaterial::InitAssetTypes()
{
	eg::RegisterAssetGenerator<StaticPropMaterialGenerator>("StaticPropMaterial", StaticPropMaterialAssetFormat);
	eg::RegisterAssetLoader("StaticPropMaterial", &StaticPropMaterial::AssetLoader, StaticPropMaterialAssetFormat);
}

size_t StaticPropMaterial::PipelineHash() const
{
	return typeid(StaticPropMaterial).hash_code() + m_backfaceCull + m_backfaceCullEditor * 2;
}

inline static eg::PipelineRef GetPipeline(const MeshDrawArgs& drawArgs, bool backfaceCull, bool backfaceCullEditor)
{
	switch (drawArgs.drawMode)
	{
	case MeshDrawMode::Game: return staticPropPipelineGame[(int)backfaceCull];
	case MeshDrawMode::ObjectFlags: return staticPropPipelineFlags[(int)backfaceCull];
	case MeshDrawMode::Editor: return staticPropPipelineEditor[(int)backfaceCullEditor];
	case MeshDrawMode::PointLightShadow: return staticPropPipelinePLShadow[(int)backfaceCull];
	default: return eg::PipelineRef();
	}
	EG_UNREACHABLE
}

bool StaticPropMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	MeshDrawArgs* mDrawArgs = static_cast<MeshDrawArgs*>(drawArgs);
	eg::PipelineRef pipeline = GetPipeline(*mDrawArgs, m_backfaceCull, m_backfaceCullEditor);
	if (pipeline.handle == nullptr)
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
	if (mDrawArgs->drawMode == MeshDrawMode::ObjectFlags && m_objectFlags == 0)
		return false;
	
	eg::TextureSubresource subresource = { 0, eg::REMAINING_SUBRESOURCE, m_textureLayer, 1 };
	
	if (mDrawArgs->drawMode == MeshDrawMode::PointLightShadow)
	{
		if (!m_castShadows || settings.shadowQuality < m_minShadowQuality)
			return false;
		cmdCtx.BindTexture(*m_albedoTexture, 0, 1, nullptr, subresource, eg::TextureBindFlags::ArrayLayerAsTexture2D);
		return true;
	}
	
	if (!m_descriptorsInitialized)
	{
		m_descriptorSet = eg::DescriptorSet(staticPropPipelineGame[(int)m_backfaceCull], 0);
		m_descriptorSetEditor = eg::DescriptorSet(staticPropPipelineEditor[(int)m_backfaceCullEditor], 0);
		m_descriptorsInitialized = true;
		
		m_descriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
		m_descriptorSet.BindTexture(*m_albedoTexture, 1, nullptr, subresource, eg::TextureBindFlags::ArrayLayerAsTexture2D);
		m_descriptorSet.BindTexture(*m_normalMapTexture, 2, nullptr, subresource, eg::TextureBindFlags::ArrayLayerAsTexture2D);
		m_descriptorSet.BindTexture(*m_miscMapTexture, 3, nullptr, subresource, eg::TextureBindFlags::ArrayLayerAsTexture2D);
	}
	
	if (mDrawArgs->drawMode == MeshDrawMode::Editor || mDrawArgs->drawMode == MeshDrawMode::Game)
	{
		cmdCtx.BindDescriptorSet(m_descriptorSet, 0);
		
		float pushConstants[] = { m_roughnessMin, m_roughnessMax, m_textureScale.x, m_textureScale.y };
		cmdCtx.PushConstants(0, sizeof(pushConstants), pushConstants);
	}
	else if (mDrawArgs->drawMode == MeshDrawMode::ObjectFlags)
	{
		FlagsPipelinePushConstantData pc = { RenderSettings::instance->viewProjection, m_objectFlags };
		
		cmdCtx.PushConstants(0, sizeof(pc), &pc);
	}
	
	return true;
}

bool StaticPropMaterial::CheckInstanceDataType(const std::type_info* instanceDataType) const
{
	return instanceDataType == &typeid(InstanceData);
}

const StaticPropMaterial& StaticPropMaterial::GetFromWallMaterial(uint32_t index)
{
	EG_ASSERT(index != 0);
	if (staticPropWallMaterials[index] == nullptr)
	{
		staticPropWallMaterials[index] = std::make_unique<StaticPropMaterial>();
		staticPropWallMaterials[index]->m_albedoTexture = &eg::GetAsset<eg::Texture>("WallTextures/Albedo");
		staticPropWallMaterials[index]->m_normalMapTexture = &eg::GetAsset<eg::Texture>("WallTextures/NormalMap");
		staticPropWallMaterials[index]->m_miscMapTexture = &eg::GetAsset<eg::Texture>("WallTextures/MiscMap");
		staticPropWallMaterials[index]->m_textureLayer = index - 1;
		staticPropWallMaterials[index]->m_roughnessMin = wallMaterials[index].minRoughness;
		staticPropWallMaterials[index]->m_roughnessMax = wallMaterials[index].maxRoughness;
		staticPropWallMaterials[index]->m_textureScale = glm::vec2(1.0f / wallMaterials[index].textureScale);
		staticPropWallMaterials[index]->m_backfaceCullEditor = true;
	}
	return *staticPropWallMaterials[index];
}
