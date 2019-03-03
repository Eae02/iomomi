#include "StaticPropMaterial.hpp"
#include "../Renderer.hpp"
#include "../GraphicsCommon.hpp"

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

static eg::Pipeline staticPropPipelineEditor;
static eg::Pipeline staticPropPipelineGame;

static void OnInit()
{
	eg::ShaderProgram shaderProgramGame;
	shaderProgramGame.AddStageFromAsset("Shaders/Common3D.vs.glsl");
	shaderProgramGame.AddStageFromAsset("Shaders/StaticModel.fs.glsl");
	
	eg::FixedFuncState fixedFuncState;
	fixedFuncState.enableDepthWrite = true;
	fixedFuncState.enableDepthTest = true;
	fixedFuncState.cullMode = eg::CullMode::Back;
	fixedFuncState.frontFaceCCW = true;
	fixedFuncState.vertexBindings[0] = { sizeof(eg::StdVertex), eg::InputRate::Vertex };
	fixedFuncState.vertexBindings[1] = { sizeof(float) * 16, eg::InputRate::Instance };
	fixedFuncState.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, offsetof(eg::StdVertex, position) };
	fixedFuncState.vertexAttributes[1] = { 0, eg::DataType::Float32, 2, offsetof(eg::StdVertex, texCoord) };
	fixedFuncState.vertexAttributes[2] = { 0, eg::DataType::SInt8Norm, 3, offsetof(eg::StdVertex, normal) };
	fixedFuncState.vertexAttributes[3] = { 0, eg::DataType::SInt8Norm, 3, offsetof(eg::StdVertex, tangent) };
	fixedFuncState.vertexAttributes[4] = { 1, eg::DataType::Float32, 4, 0 * sizeof(float) * 4 };
	fixedFuncState.vertexAttributes[5] = { 1, eg::DataType::Float32, 4, 1 * sizeof(float) * 4 };
	fixedFuncState.vertexAttributes[6] = { 1, eg::DataType::Float32, 4, 2 * sizeof(float) * 4 };
	fixedFuncState.vertexAttributes[7] = { 1, eg::DataType::Float32, 4, 3 * sizeof(float) * 4 };
	
	fixedFuncState.depthFormat = Renderer::DEPTH_FORMAT;
	fixedFuncState.attachments[0].format = eg::Format::R8G8B8A8_UNorm;
	fixedFuncState.attachments[1].format = eg::Format::R8G8B8A8_UNorm;
	staticPropPipelineGame = shaderProgramGame.CreatePipeline(fixedFuncState);
	
	eg::ShaderProgram shaderProgramEditor;
	shaderProgramEditor.AddStageFromAsset("Shaders/Common3D.vs.glsl");
	shaderProgramEditor.AddStageFromAsset("Shaders/StaticModel-Editor.fs.glsl");
	
	fixedFuncState.depthFormat = eg::Format::DefaultDepthStencil;
	fixedFuncState.cullMode = eg::CullMode::None;
	fixedFuncState.attachments[0].format = eg::Format::DefaultColor;
	fixedFuncState.attachments[1].format = eg::Format::Undefined;
	staticPropPipelineEditor = shaderProgramEditor.CreatePipeline(fixedFuncState);
}

static void OnShutdown()
{
	staticPropPipelineEditor.Destroy();
	staticPropPipelineGame.Destroy();
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

eg::PipelineRef StaticPropMaterial::GetPipeline(ObjectMaterial::PipelineType pipelineType) const
{
	if (pipelineType == ObjectMaterial::PipelineType::Editor)
		return staticPropPipelineEditor;
	return staticPropPipelineGame;
}

void StaticPropMaterial::Bind(ObjectMaterial::PipelineType boundPipeline) const
{
	eg::DC.BindTexture(*m_albedoTexture, 1, &GetCommonTextureSampler());
	eg::DC.BindTexture(*m_normalMapTexture, 2, &GetCommonTextureSampler());
	eg::DC.BindTexture(*m_miscMapTexture, 3, &GetCommonTextureSampler());
	
	float pushConstants[] = {m_roughnessMin, m_roughnessMax, m_textureScale.x, m_textureScale.y};
	eg::DC.PushConstants(0, sizeof(pushConstants), pushConstants);
}
