#include "StaticPropMaterial.hpp"
#include "MeshDrawArgs.hpp"
#include "../Renderer.hpp"
#include "../GraphicsCommon.hpp"
#include "../RenderSettings.hpp"

#include <fstream>

static const eg::AssetFormat StaticPropMaterialAssetFormat { "StaticPropMaterial", 1 };

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
		
		std::string albedoPath = rootYaml["albedo"].as<std::string>(std::string());
		std::string normalMapPath = rootYaml["normalMap"].as<std::string>(std::string());
		std::string miscMapPath = rootYaml["miscMap"].as<std::string>(std::string());
		
		if (albedoPath.empty())
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
		
		generateContext.AddLoadDependency(std::move(albedoPath));
		generateContext.AddLoadDependency(std::move(normalMapPath));
		generateContext.AddLoadDependency(std::move(miscMapPath));
		
		return true;
	}
};

static eg::Pipeline staticPropPipelineEditor;
static eg::Pipeline staticPropPipelineGame;

static void OnInit()
{
	eg::PipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModule>("Shaders/Common3D.vs.glsl").Handle();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModule>("Shaders/StaticModel.fs.glsl").Handle();
	pipelineCI.enableDepthWrite = true;
	pipelineCI.enableDepthTest = true;
	pipelineCI.cullMode = eg::CullMode::Back;
	pipelineCI.frontFaceCCW = true;
	pipelineCI.vertexBindings[0] = { sizeof(eg::StdVertex), eg::InputRate::Vertex };
	pipelineCI.vertexBindings[1] = { sizeof(float) * 16, eg::InputRate::Instance };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, offsetof(eg::StdVertex, position) };
	pipelineCI.vertexAttributes[1] = { 0, eg::DataType::Float32, 2, offsetof(eg::StdVertex, texCoord) };
	pipelineCI.vertexAttributes[2] = { 0, eg::DataType::SInt8Norm, 3, offsetof(eg::StdVertex, normal) };
	pipelineCI.vertexAttributes[3] = { 0, eg::DataType::SInt8Norm, 3, offsetof(eg::StdVertex, tangent) };
	pipelineCI.vertexAttributes[4] = { 1, eg::DataType::Float32, 4, 0 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[5] = { 1, eg::DataType::Float32, 4, 1 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[6] = { 1, eg::DataType::Float32, 4, 2 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[7] = { 1, eg::DataType::Float32, 4, 3 * sizeof(float) * 4 };
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	pipelineCI.numColorAttachments = 2;
	staticPropPipelineGame = eg::Pipeline::Create(pipelineCI);
	staticPropPipelineGame.FramebufferFormatHint(Renderer::GEOMETRY_FB_FORMAT);
	
	pipelineCI.numColorAttachments = 1;
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModule>("Shaders/StaticModel-Editor.fs.glsl").Handle();
	pipelineCI.cullMode = eg::CullMode::None;
	staticPropPipelineEditor = eg::Pipeline::Create(pipelineCI);
	staticPropPipelineEditor.FramebufferFormatHint(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil);
}

static void OnShutdown()
{
	staticPropPipelineEditor.Destroy();
	staticPropPipelineGame.Destroy();
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
	
	return true;
}

void StaticPropMaterial::InitAssetTypes()
{
	eg::RegisterAssetGenerator<StaticPropMaterialGenerator>("StaticPropMaterial", StaticPropMaterialAssetFormat);
	eg::RegisterAssetLoader("StaticPropMaterial", &StaticPropMaterial::AssetLoader, StaticPropMaterialAssetFormat);
}

size_t StaticPropMaterial::PipelineHash() const
{
	return typeid(StaticPropMaterial).hash_code();
}

void StaticPropMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	MeshDrawArgs* mDrawArgs = static_cast<MeshDrawArgs*>(drawArgs);
	cmdCtx.BindPipeline(mDrawArgs->editor ? staticPropPipelineEditor : staticPropPipelineGame);
}

void StaticPropMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	if (!m_descriptorsInitialized)
	{
		m_descriptorSetGame = eg::DescriptorSet(staticPropPipelineGame, 0);
		m_descriptorSetEditor = eg::DescriptorSet(staticPropPipelineEditor, 0);
		m_descriptorsInitialized = true;
		
		m_descriptorSetGame.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
		m_descriptorSetGame.BindTexture(*m_albedoTexture, 1, &GetCommonTextureSampler());
		m_descriptorSetGame.BindTexture(*m_normalMapTexture, 2, &GetCommonTextureSampler());
		m_descriptorSetGame.BindTexture(*m_miscMapTexture, 3, &GetCommonTextureSampler());
		
		m_descriptorSetEditor.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
		m_descriptorSetEditor.BindTexture(*m_albedoTexture, 1, &GetCommonTextureSampler());
		m_descriptorSetEditor.BindTexture(*m_normalMapTexture, 2, &GetCommonTextureSampler());
		m_descriptorSetEditor.BindTexture(*m_miscMapTexture, 3, &GetCommonTextureSampler());
	}
	
	MeshDrawArgs* mDrawArgs = static_cast<MeshDrawArgs*>(drawArgs);
	cmdCtx.BindDescriptorSet(mDrawArgs->editor ? m_descriptorSetEditor : m_descriptorSetGame);
	
	float pushConstants[] = {m_roughnessMin, m_roughnessMax, m_textureScale.x, m_textureScale.y};
	cmdCtx.PushConstants(0, sizeof(pushConstants), pushConstants);
}
