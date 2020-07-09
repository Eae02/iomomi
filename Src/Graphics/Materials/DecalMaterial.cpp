#include "DecalMaterial.hpp"
#include "MeshDrawArgs.hpp"
#include "../GraphicsCommon.hpp"
#include "../RenderSettings.hpp"

#include <fstream>

static eg::Pipeline decalsGamePipeline;
static eg::Pipeline decalsGamePipelineInheritNormals;
static eg::Pipeline decalsEditorPipeline;

static eg::Buffer decalVertexBuffer;

static float decalVertexData[] = 
{
	-1, -1,
	1, -1,
	-1, 1,
	1, 1
};

static void OnInit()
{
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
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	pipelineCI.vertexBindings[0] = { sizeof(float) * 2, eg::InputRate::Vertex };
	pipelineCI.vertexBindings[1] = { sizeof(glm::mat4), eg::InputRate::Instance };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 2, 0 };
	pipelineCI.vertexAttributes[1] = { 1, eg::DataType::Float32, 4, 0 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[2] = { 1, eg::DataType::Float32, 4, 1 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[3] = { 1, eg::DataType::Float32, 4, 2 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[4] = { 1, eg::DataType::Float32, 4, 3 * sizeof(float) * 4 };
	pipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFunc::Add,
		/* srcColor */ eg::BlendFactor::SrcAlpha,
		/* srcAlpha */ eg::BlendFactor::ConstantAlpha,
		/* dstColor */ eg::BlendFactor::OneMinusSrcAlpha,
		/* dstAlpha */ eg::BlendFactor::OneMinusSrcAlpha);
	pipelineCI.blendStates[1] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFunc::Add,
		/* srcColor */ eg::BlendFactor::SrcAlpha,
		/* srcAlpha */ eg::BlendFactor::Zero,
		/* dstColor */ eg::BlendFactor::OneMinusSrcAlpha,
		/* dstAlpha */ eg::BlendFactor::OneMinusSrcAlpha);
	pipelineCI.blendConstants[3] = 1.0f;
	pipelineCI.label = "DecalsGame";
	decalsGamePipeline = eg::Pipeline::Create(pipelineCI);
	
	pipelineCI.blendStates[1].colorWriteMask = eg::ColorWriteMask::A | eg::ColorWriteMask::B;
	decalsGamePipelineInheritNormals = eg::Pipeline::Create(pipelineCI);
	
	pipelineCI.fragmentShader = fs.GetVariant("VEditor");
	pipelineCI.numColorAttachments = 1;
	pipelineCI.blendStates[1] = { };
	pipelineCI.label = "DecalsEditor";
	decalsEditorPipeline = eg::Pipeline::Create(pipelineCI);
	
	decalVertexBuffer = eg::Buffer(eg::BufferFlags::VertexBuffer, sizeof(decalVertexData), decalVertexData);
	decalVertexBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
}

static void OnShutdown()
{
	decalsGamePipeline.Destroy();
	decalsGamePipelineInheritNormals.Destroy();
	decalsEditorPipeline.Destroy();
	decalVertexBuffer.Destroy();
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

class DecalMaterialGenerator : public eg::AssetGenerator
{
public:
	bool Generate(eg::AssetGenerateContext& generateContext) override
	{
		std::string relSourcePath = generateContext.RelSourcePath();
		std::string sourcePath = generateContext.FileDependency(relSourcePath);
		std::ifstream stream(sourcePath);
		EG_ASSERT(stream);
		
		YAML::Node rootYaml = YAML::Load(stream);
		
		const float roughness = rootYaml["roughness"].as<float>(1.0f);
		const float opacity = rootYaml["opacity"].as<float>(1.0f);
		const bool inheritNormals = rootYaml["inheritNormals"].as<bool>(false);
		
		std::string albedoPath = rootYaml["albedo"].as<std::string>(std::string());
		std::string normalMapPath = rootYaml["normalMap"].as<std::string>(std::string());
		
		if (albedoPath.empty())
		{
			eg::Log(eg::LogLevel::Error, "as", "Invalid decal material '{0}', missing 'albedo' entry", relSourcePath);
			return false;
		}
		
		eg::BinWriteString(generateContext.outputStream, albedoPath);
		eg::BinWriteString(generateContext.outputStream, normalMapPath);
		eg::BinWrite(generateContext.outputStream, roughness);
		eg::BinWrite(generateContext.outputStream, opacity);
		eg::BinWrite<uint8_t>(generateContext.outputStream, inheritNormals);
		
		generateContext.AddLoadDependency(std::move(albedoPath));
		generateContext.AddLoadDependency(std::move(normalMapPath));
		
		return true;
	}
};

static const eg::AssetFormat DecalMaterialAssetFormat { "DecalMaterial", 2 };

void DecalMaterial::InitAssetTypes()
{
	eg::RegisterAssetGenerator<DecalMaterialGenerator>("DecalMaterial", DecalMaterialAssetFormat);
	eg::RegisterAssetLoader("DecalMaterial", &DecalMaterial::AssetLoader, DecalMaterialAssetFormat);
}

bool DecalMaterial::AssetLoader(const eg::AssetLoadContext& loadContext)
{
	eg::MemoryStreambuf memoryStreambuf(loadContext.Data());
	std::istream stream(&memoryStreambuf);
	
	std::string albedoTextureName = eg::BinReadString(stream);
	std::string normalMapTextureName = eg::BinReadString(stream);
	
	std::string albedoTexturePath = eg::Concat({ loadContext.DirPath(), albedoTextureName} );
	std::string normalMapTexturePath = eg::Concat({ loadContext.DirPath(), normalMapTextureName} );
	
	DecalMaterial& material = loadContext.CreateResult<DecalMaterial>(
		eg::GetAsset<eg::Texture>(albedoTexturePath),
		eg::GetAsset<eg::Texture>(normalMapTexturePath)
	);
	
	material.m_roughness = eg::BinRead<float>(stream);
	material.m_opacity = eg::BinRead<float>(stream);
	material.m_inheritNormals = eg::BinRead<uint8_t>(stream);
	
	return true;
}

DecalMaterial::DecalMaterial(const eg::Texture& albedoTexture, const eg::Texture& normalMapTexture)
	: m_albedoTexture(albedoTexture), m_normalMapTexture(normalMapTexture),
	  m_aspectRatio(albedoTexture.Width() / (float)albedoTexture.Height()) { }

size_t DecalMaterial::PipelineHash() const
{
	return typeid(DecalMaterial).hash_code() + m_inheritNormals;
}

bool DecalMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	MeshDrawArgs* mDrawArgs = reinterpret_cast<MeshDrawArgs*>(drawArgs);
	
	if (mDrawArgs->drawMode == MeshDrawMode::Game)
	{
		cmdCtx.BindPipeline(m_inheritNormals ? decalsGamePipelineInheritNormals : decalsGamePipeline);
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
	if (!m_descriptorSetInitialized)
	{
		m_descriptorSet = eg::DescriptorSet(decalsGamePipeline, 0);
		
		m_descriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
		m_descriptorSet.BindTexture(m_albedoTexture, 1, &GetCommonTextureSampler());
		m_descriptorSet.BindTexture(m_normalMapTexture, 2, &GetCommonTextureSampler());
		
		m_descriptorSetInitialized = true;
	}
	
	float pc[2];
	pc[0] = m_roughness;
	pc[1] = m_opacity;
	
	cmdCtx.PushConstants(0, sizeof(pc), &pc);
	
	cmdCtx.BindDescriptorSet(m_descriptorSet, 0);
	
	return true;
}

eg::MeshBatch::Mesh DecalMaterial::GetMesh()
{
	eg::MeshBatch::Mesh mesh;
	mesh.firstIndex = 0;
	mesh.firstVertex = 0;
	mesh.indexBuffer = eg::BufferRef();
	mesh.numElements = 4;
	mesh.vertexBuffer = decalVertexBuffer;
	return mesh;
}
